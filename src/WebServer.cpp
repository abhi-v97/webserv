#include <cerrno>
#include <csignal> // used for std::signal
#include <cstdio>
#include <cstring>
#include <fcntl.h> // used for fcntl(), believe it or not
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "CgiHandler.hpp"
#include "Logger.hpp"
#include "ResponseBuilder.hpp"
#include "WebServer.hpp"

/** \var gSignal
    Global variable used to store signal information
*/
int gSignal = 0;

/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

WebServer::WebServer(std::string ipAddress, int port)
	: mIpAddress(ipAddress), mPort(port), mListenSocket(), mSocketAddress(),
	  mSocketAdddressLen(sizeof(mSocketAddress)), mLog(Logger::getInstance())
{
	mSocketAddress.sin_family = AF_INET;
	mSocketAddress.sin_port = htons(mPort);
	// TODO: inet_addr not an allowed function?
	mSocketAddress.sin_addr.s_addr = inet_addr(mIpAddress.c_str());

	// setup signal handling
	// TODO: how should signals work with multiple servers?
	std::signal(SIGINT, SIG_IGN);
	std::signal(SIGINT, signalHandler);
	if (startServer() != 0)
	{
		mLog->log(NOTICE, "Failed to start server, shutting down");
	}
}

/*
** -------------------------------- DESTRUCTOR --------------------------------
*/

WebServer::~WebServer()
{
	// TODO: segfaults, but probably not worth worrying about
	mLog->log(NOTICE, "destructor called, web server shutting down");
	std::signal(SIGINT, SIG_DFL);
	close(mListenSocket);
	for (int i = 0; i < mPollFdVector.size(); i++)
	{
		close(mPollFdVector[i].fd);
	}
}

/*
** --------------------------------- METHODS ----------------------------------
*/

/**
    \brief Server socket and port setup

    creates the listening socket, sets it to nonblocking, and binds the socket
    to the server port
*/
int WebServer::startServer()
{
	// set up the listening socket
	mListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (mListenSocket < 0)
	{
		mLog->log(ERROR,
		          std::string("socket() failed: ") + std::strerror(errno));
		return 1;
	}
	mLog->log(NOTICE, std::string("Socket created with value: ") +
	                     numToString(mListenSocket));
	// OS doesn't immediately free the port, which causes web server to hang
	// if you close and reopen it quickly. This tells our server that we can
	// reuse the port.
	int enable = 1;
	if (setsockopt(mListenSocket, SOL_SOCKET, SO_REUSEADDR, &enable,
	               sizeof(int)) < 0)
	{
		mLog->log(ERROR, std::string("setsockopt(SO_REUSEADDR) failed: ") +
		                     std::strerror(errno));
		return 1;
	}

	// set the socket to be non-blocking
	setNonBlockingFlag(mListenSocket);

	// bind socket and server port
	if (bind(mListenSocket, (sockaddr *)&mSocketAddress, mSocketAdddressLen) <
	    0)
	{
		mLog->log(ERROR, std::string("bind() failed: ") + std::strerror(errno));
		return 1;
	}
	mLog->log(NOTICE, std::string("bind success: ") + numToString(mIpAddress) +
	                      std::string(":") + numToString(mPort));
	return 0;
}

// call this after construction to "start" the server
// currently, webserver starts listening and essentially "waits" until it hears
// a request. Then it grabs the request (no parsing yet) and sends some data
// back (test.html for now). Then it shuts down.
/**
    \brief Server starts listening for incoming connections

    Kickstarts the server loop, listening for incoming connections and handles
    them in a rudimentary manner

    Server keeps listening until SIGINT is received
*/
void WebServer::startListen()
{
	if (listen(mListenSocket, 8) < 0)
	{
		mLog->log(ERROR,
		          std::string("listen() failed: ") + std::strerror(errno));
		return;
	}

	struct pollfd listenStruct = {mListenSocket, POLLIN};
	mPollFdVector.push_back(listenStruct);
	while (gSignal == 0)
	{
		int pollNum = poll(mPollFdVector.data(), mPollFdVector.size(), 1);
		for (int i = static_cast<int>(mPollFdVector.size()) - 1; i >= 0; i--)
		{
			if (mPollFdVector[i].fd == mListenSocket &&
			    (mPollFdVector[i].revents & POLLIN))
			{
				// if this condition is true, it means we have a new connection
				acceptConnection();
			}
			else if (mPollFdVector[i].revents & POLLIN)
			{
				// this means we're now dealing with a client
				parseRequest(i);
			}
			if (mPollFdVector[i].revents & POLLOUT)
			{
				// if we have enough data, send reponse to client
				if (sendResponse(mPollFdVector[i].fd))
				{
					// if all data has been sent, remove POLLOUT flag
					mPollFdVector[i].events &= ~POLLOUT;
				}
			}
		}
	}
}

/**
    \brief Handles accepting new clients

    Accepts the client connection, then stores client info and updates the
    mClients container with new client
*/
void WebServer::acceptConnection()
{
	int newSocket = 0;
	struct sockaddr_in clientAddr = {};
	socklen_t clientLen = sizeof(clientAddr);

	newSocket =
		accept(mListenSocket, (sockaddr *)&clientAddr, &mSocketAdddressLen);
	if (newSocket < 0)
	{
		mLog->log(ERROR,
		          std::string("accept() failed: ") + std::strerror(errno));
	}
	mLog->log(NOTICE, std::string("accepted client with fd: ") +
	                      numToString(newSocket));

	// get client IP from sockaddr struct, store it as std::string
	std::ostringstream clientIp;
	clientIp << int(clientAddr.sin_addr.s_addr & 0xFF) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF00) >> 8) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF0000) >> 16) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF000000) >> 24)
			 << std::endl;

	// update the pollfd struct
	struct pollfd newClient = {newSocket, POLLIN};
	mPollFdVector.push_back(newClient);

	// update clientState with new client
	ClientState state;
	state.bytesRead = 0;
	state.bytesSent = 0;
	state.fd = newSocket;
	state.clientIp = clientIp.str();
	mClients[newSocket] = state;
}

/**
    \brief Receives the client request and sends back a generic response, for
   now
*/
void WebServer::parseRequest(int clientNum)
{
	ClientState &client = mClients[mPollFdVector[clientNum].fd];
	char buffer[4096] = {0};

	client.bytesRead = recv(mPollFdVector[clientNum].fd, buffer, 4096, 0);
	client.request += buffer;
	if (client.bytesRead < 0)
	{
		mLog->log(WARNING, "failed to read client request");
	}
	else if (client.bytesRead == 0)
	{
		// request of zero bytes received, shut down the connection
		// TODO: other conditions and revents can also require us to close the
		// connection; turn this into a function, find all other applicable
		// revents
		mLog->log(
			WARNING,
			std::string("recv(): zero bytes received, closing connection ") +
				numToString(mPollFdVector[clientNum].fd));
		close(mPollFdVector[clientNum].fd);
		mClients.erase(mPollFdVector[clientNum].fd);
		mPollFdVector.erase(mPollFdVector.begin() + clientNum);
	}
	else if (client.request.find("\r\n\r\n") != std::string::npos)
	{
		mPollFdVector[clientNum].events |= POLLOUT;
		generateResponse(mPollFdVector[clientNum].fd);
		client.request.erase();
	}
}

/**
    Placeholder function, generates a blank page to send to client.
*/
std::string WebServer::defaultResponse()
{
	std::ifstream htmlFile("test.html");
	std::ostringstream body;
	std::ostringstream response;

	body << htmlFile.rdbuf();
	response << "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: "
			 << body.str().size() << "\n\n"
			 << body.str();
	return response.str();
}

/**
    \brief Placeholder function, generates a dummy message as a response
    regardless of what the client requests

    \param clientFd client socket FD to send the response to
*/
void WebServer::generateResponse(int clientFd)
{
	ClientState &client = mClients[clientFd];
	ssize_t bytes = 0;
	int isCGI = 1;
	if (isCGI)
	{
		ResponseBuilder dummyObj = ResponseBuilder();
		CgiHandler cgiObj = CgiHandler();

		// TODO: change this to accept other CGI requests
		cgiObj.execute("hello.py");
		client.response = dummyObj.buildCgiResponse(cgiObj.getOutFd());
	}
	else
	{
		client.response = defaultResponse();
	}
}

/**
    Attempts to send response data to client

    \param clientFd socket FD of the client is receiving the response
*/
bool WebServer::sendResponse(int clientFd)
{
	ClientState &client = mClients[clientFd];
	ssize_t bytes = 0;

	// send everything in m_serverResponse to client
	while (client.bytesSent < client.response.size())
	{
		bytes = send(clientFd, client.response.c_str() + client.bytesSent,
		             client.response.size() - client.bytesSent, MSG_NOSIGNAL);
		if (bytes < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				break;
			}
			else
			{
				return false;
			}
		}
		client.bytesSent += bytes;
	}
	if (client.bytesSent != client.response.size())
	{
		mLog->log(WARNING, "send() failed");
	}
	return false;
}

/*
** --------------------------------- ACCESSOR ---------------------------------
*/

/* ************************************************************************** */

/**
    Helper function which adds the non-blocking flag to the listen socket.

    \param socketFd
*/
bool setNonBlockingFlag(int socketFd)
{
	int flags = fcntl(socketFd, F_GETFL, 0);
	if (flags == -1)
	{
		std::cerr << "get flag fcntl operation failed" << std::endl;
		return false;
	}
	int status = fcntl(socketFd, F_SETFL, flags | O_NONBLOCK);
	if (status == -1)
	{
		std::cerr << "set flag fcntl operation failed" << std::endl;
		return false;
	}
	return true;
}

/**
    Helper function which changes the global signal variable when a signal is
    received

    \param sig signal value
*/
void signalHandler(int sig)
{
	gSignal = sig;
}
