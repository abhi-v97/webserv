#include <cerrno>
#include <csignal> // used for std::signal
#include <cstdio>
#include <cstring>
#include <fcntl.h> // used for fcntl(), believe it or not
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "CgiHandler.hpp"
#include "Logger.hpp"
#include "RequestParser.hpp"
#include "ResponseBuilder.hpp"
#include "WebServer.hpp"
#include "configParser.hpp"

#define LISTEN_REQUESTS 8

/** \var gSignal
	Global variable used to store signal information
*/
int gSignal = 0;

/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

WebServer::WebServer(std::string ipAddress, const std::vector<ServerConfig> &srv)
	: mIpAddress(ipAddress), mLog(Logger::getInstance())

{
	for (std::vector<ServerConfig>::const_iterator it = srv.begin(); it != srv.end(); it++)
	{
		std::vector<int> ports = (*it).listenPorts;
		for (std::vector<int>::const_iterator it = ports.begin(); it != ports.end(); it++)
		{
			sockaddr_in socketAddress = sockaddr_in();
			socketAddress.sin_family = AF_INET;
			socketAddress.sin_port = htons(*it);
			socketAddress.sin_addr.s_addr = inet_addr(mIpAddress.c_str());
			if (bindPort(socketAddress) != 0)
			{
				mLog->log(NOTICE, "Failed to bind port: " + numToString(*it));
				continue;
			}
			mSocketAddressStruct.push_back(socketAddress);
		}
	}
	if (mSocketAddressStruct.size() < 1)
	{
		mLog->log(ERROR, "Failed to bind given ports");
		throw std::runtime_error("No ports bound, server shutting down");
	}
}

// old code, use it to initialise a single ip address + port combo
WebServer::WebServer(std::string ipAddress, int port)
	: mIpAddress(ipAddress), mLog(Logger::getInstance())
{
	sockaddr_in socketAddress = sockaddr_in();
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_port = htons(port);
	// TODO: inet_addr not an allowed function?
	socketAddress.sin_addr.s_addr = inet_addr(mIpAddress.c_str());

	// setup signal handling
	std::signal(SIGINT, SIG_IGN);
	std::signal(SIGINT, signalHandler);
	if (bindPort(socketAddress) != 0)
	{
		mLog->log(NOTICE, "Failed to start server, shutting down");
	}
	mSocketAddressStruct.push_back(socketAddress);
}

/*
** -------------------------------- DESTRUCTOR --------------------------------
*/

WebServer::~WebServer()
{
	mLog->log(NOTICE, "destructor called, web server shutting down");
	std::signal(SIGINT, SIG_DFL);
	for (size_t i = 0; i < mSocketVector.size(); i++)
	{
		close(mSocketVector.at(i));
	}
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

	\param socketStruct sockaddr_in struct that contains listening port info
*/
int WebServer::bindPort(sockaddr_in socketStruct)
{
	// set up the listening socket
	int mSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (mSocket < 0)
	{
		mLog->log(ERROR, std::string("socket() failed: ") + std::strerror(errno));
		return 1;
	}
	mLog->log(NOTICE,
			  std::string("Socket created with value: ") +
				  numToString(ntohs(socketStruct.sin_port)));

	// OS doesn't immediately free the port, which causes web server to hang
	// if you close and reopen it quickly. This tells our server that we can
	// reuse the port.
	int enable = 1;
	if (setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
	{
		mLog->log(ERROR, std::string("setsockopt(SO_REUSEADDR) failed: ") + std::strerror(errno));
		return 1;
	}

	// set the socket to be non-blocking
	setNonBlockingFlag(mSocket);

	// bind socket and server port
	if (bind(mSocket, (sockaddr *) &socketStruct, sizeof(socketStruct)) < 0)
	{
		mLog->log(ERROR, std::string("bind() failed: ") + std::strerror(errno));
		return 1;
	}
	mLog->log(NOTICE,
			  std::string("bind success: ") + numToString(mIpAddress) + std::string(":") +
				  numToString(socketStruct.sin_port));
	mSocketVector.push_back(mSocket);
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
	mPollFdVector.reserve(mSocketVector.size());
	for (int i = 0; i < mSocketVector.size(); i++)
	{
		if (listen(mSocketVector.at(i), LISTEN_REQUESTS) < 0)
		{
			mLog->log(ERROR, std::string("listen() failed: ") + std::strerror(errno));
			return;
		}
		struct pollfd listenStruct = {mSocketVector.at(i), POLLIN, 0};
		mPollFdVector.push_back(listenStruct);
	}

	// listen ports get added before clients, so keep track of how many we have
	const int listenCount = static_cast<int>(mSocketVector.size());

	while (gSignal == 0)
	{
		int pollNum = poll(mPollFdVector.data(), static_cast<int>(mPollFdVector.size()), 1);
		if (pollNum < 0)
		{
			mLog->log(ERROR, std::string("poll() failed: ") + std::strerror(errno));
		}
		for (int i = static_cast<int>(mPollFdVector.size()) - 1; i >= 0; i--)
		{
			if (i < listenCount && (mPollFdVector[i].revents & POLLIN))
			{
				// if this condition is true, it means we have a new connection
				acceptConnection(mPollFdVector[i].fd);
			}
			else if (mPollFdVector[i].revents & POLLIN)
			{
				// this means we're now dealing with a client
				readFromSocket(i);
				parseRequest(i);
			}
			else if (mPollFdVector[i].revents & POLLOUT)
			{
				if (generateResponse(i) == true)
				{
					// if we have enough data, send reponse to client
					if (sendResponse(i) == true)
					{
						mClients[mPollFdVector[i].fd].responseReady = false;
						parseRequest(i);
					}
					// if client requests to keep connection alive
					if (mClients[mPollFdVector[i].fd].keepAlive == false)
						closeConnection(i);
					continue;
				}
				// if all data has been sent, remove POLLOUT flag
				mPollFdVector[i].events &= ~POLLOUT;
			}
		}
	}
}

/**
	\brief Handles accepting new clients

	Accepts the client connection, t hen stores client info and updates the
	mClients container with new client

	\param socket The listening socket that client used to contact the server
*/
void WebServer::acceptConnection(int listenFd)
{
	int				   newSocket = 0;
	struct sockaddr_in clientAddr = {};
	socklen_t		   clientLen = sizeof(clientAddr);

	newSocket = accept(listenFd, (sockaddr *) &clientAddr, &clientLen);
	if (newSocket < 0)
	{
		mLog->log(ERROR, std::string("accept() failed: ") + std::strerror(errno));
	}
	mLog->log(NOTICE, std::string("accepted client with fd: ") + numToString(newSocket));

	// set acceptd client socket to be non-blocking
	if (!setNonBlockingFlag(newSocket))
	{
		mLog->log(WARNING, std::string("failed to set client as non-blocking"));
	}

	// get client IP from sockaddr struct, store it as std::string
	std::ostringstream clientIp;
	clientIp << int(clientAddr.sin_addr.s_addr & 0xFF) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF00) >> 8) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF0000) >> 16) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF000000) >> 24) << std::endl;

	// update the pollfd struct
	struct pollfd newClient = {newSocket, POLLIN, 0};
	mPollFdVector.push_back(newClient);

	// set the socket to be non-blocking
	setNonBlockingFlag(newSocket);

	// update clientState with new client
	ClientState state;
	state.bytesRead = 0;
	state.bytesSent = 0;
	state.fd = newSocket;
	state.clientIp = clientIp.str();
	state.parser = RequestParser();
	state.cgiObj = CgiHandler();
	state.responseReady = false;
	state.keepAlive = false;
	mClients[newSocket] = state;
}

void WebServer::closeConnection(int clientNum)
{
	mLog->log(NOTICE,
			  std::string("closing connection ") + numToString(mPollFdVector[clientNum].fd));
	close(mPollFdVector[clientNum].fd);
	mClients.erase(mPollFdVector[clientNum].fd);
	mPollFdVector.erase(mPollFdVector.begin() + clientNum);
}

/**
	\brief Reads socket using recv in 4kb chunks, then add it to std::string request in ClientState
   for parsing.
*/
void WebServer::readFromSocket(int clientNum)
{
	ClientState &client = mClients[mPollFdVector[clientNum].fd];
	char		 buffer[4096];
	ssize_t		 bytesRead;

	// don't read next data until you are finished with all requests from the previous read
	if (client.parser.getParsingFinished())
		return;

	bytesRead = recv(mPollFdVector[clientNum].fd, buffer, 4096, 0);
	if (bytesRead < 0)
	{
		mLog->log(WARNING, "recv() failed to read from socket");
		return;
	}
	else if (bytesRead == 0)
	{
		mLog->log(NOTICE,
				  std::string("recv(): zero bytes received, closing connection ") +
					  numToString(client.fd));
		closeConnection(clientNum);
		return;
	}
	client.request.append(buffer, bytesRead);
}

void WebServer::parseRequest(int clientNum)
{
	ClientState &client = mClients[mPollFdVector[clientNum].fd];

	if (client.request.empty())
		return;

	RequestParser &requestObj = client.parser;

	if (requestObj.parse(client.request) == false)
	{
		mLog->log(ERROR,
				  std::string("invalid request, closing connection ") + numToString(client.fd));
		closeConnection(clientNum);
		return;
	}

	if (requestObj.getParsingFinished() == true)
		mPollFdVector[clientNum].events |= POLLOUT;
}

bool WebServer::checkCgi(int clientNum)
{
	ClientState &client = mClients[mPollFdVector[clientNum].fd];
	std::string	 uri = client.parser.getUri();
	size_t		 extStart = uri.rfind('.');

	if (extStart == std::string::npos)
		return (false);
	std::string extStr = uri.substr(extStart);
	if (extStr == ".py")
	{
		client.cgiObj.setCgiType(PYTHON);
		return (true);
	}
	else if (extStr == ".sh")
	{
		client.cgiObj.setCgiType(SHELL);
		return (true);
	}

	return (false);
}

/**
	\brief Placeholder function, generates a dummy message as a response
	regardless of what the client requests

	\param clientNum client socket FD to send the response to
*/
bool WebServer::generateResponse(int clientNum)
{
	ClientState &client = mClients[mPollFdVector[clientNum].fd];
	ssize_t		 bytes = 0;
	int			 isCGI = 0;

	if (client.responseReady == true)
		return (true);
	if (client.parser.getParsingFinished() == false)
		return (false);
	client.responseReady = false;
	if (isCGI)
	{
		ResponseBuilder dummyObj = ResponseBuilder();
		CgiHandler	   &cgiObj = client.cgiObj;

		// TODO: change this to accept other CGI requests
		cgiObj.execute("hello.py");
		ResponseBuilder &responseObj = client.responseObj;

		int outfd = client.cgiObj.getOutFd();
		if (responseObj.readCgiResponse(outfd))
		{
			mPollFdVector[clientNum].events |= POLLOUT;
		}
		client.response = client.responseObj.getResponse();
	}
	else
	{
		client.response = client.responseObj.buildResponse();
	}
	client.keepAlive = client.parser.keepAlive();
	client.parser.reset();
	client.responseReady = true;
	client.bytesSent = 0;
	return (true);
}

/**
	Attempts to send response data to client

	\param clientFd socket FD of the client is receiving the response
*/
bool WebServer::sendResponse(int clientNum)
{
	ClientState &client = mClients[mPollFdVector[clientNum].fd];
	ssize_t		 bytes = 0;

	while (client.bytesSent < client.response.size())
	{
		bytes = send(client.fd,
					 client.response.c_str() + client.bytesSent,
					 client.response.size() - client.bytesSent,
					 MSG_NOSIGNAL);
		if (bytes < 0)
			return (false);
		client.bytesSent += bytes;
	}
	// return true if done, false if not yet finished
	return client.bytesSent == client.response.size();
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
