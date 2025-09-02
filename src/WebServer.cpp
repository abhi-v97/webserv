#include <cerrno>
#include <csignal> // used for std::signal
#include <cstdio>
#include <cstring>
#include <fcntl.h> // used for fcntl(), believe it or not
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "CgiHandler.hpp"
#include "ResponseBuilder.hpp"
#include "WebServer.hpp"

// global variable used for signals
int gSignal = 0;

// TODO: the formatter insists on rearranging header files to be in ascending
// orde
/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

WebServer::WebServer()
	: mPort(), mListenSocket(), mSocketAddress(), mSocketAdddressLen()
{
}

WebServer::WebServer(const WebServer &src)
	: mPort(), mListenSocket(), mSocketAddress(), mSocketAdddressLen()
{
	(void)src;
}

WebServer::WebServer(std::string ipAddress, int port)
	: mIpAddress(ipAddress), mPort(port), mListenSocket(), mSocketAddress(),
	  mSocketAdddressLen(sizeof(mSocketAddress))
{
	mSocketAddress.sin_family = AF_INET;
	mSocketAddress.sin_port = htons(mPort);
	// TODO: inet_addr not an allowed function?
	mSocketAddress.sin_addr.s_addr = inet_addr(mIpAddress.c_str());

	// setup signal handling
	// TODO: how should signals work with multiple servers?
	std::signal(SIGINT, SIG_IGN);
	std::signal(SIGINT, signalHandler);
	if (startServer() > 0)
	{
		std::cerr << "cannot connect to socket: " << mPort << ": "
				  << std::strerror(errno) << std::endl;
	}
}

/*
** -------------------------------- DESTRUCTOR --------------------------------
*/

WebServer::~WebServer()
{
	closeServer();
}

/*
** --------------------------------- OVERLOAD ---------------------------------
*/

WebServer &WebServer::operator=(const WebServer &obj)
{
	if (this == &obj)
	{
		return *this;
	}
	this->mIpAddress = obj.mIpAddress;
	this->mPort = obj.mPort;
	this->mSocketAdddressLen = obj.mSocketAdddressLen;
	return *this;
}

std::ostream &operator<<(std::ostream &outf, const WebServer &obj)
{
	outf << "42 Webserv" << std::endl;
	return outf;
}

/*
** --------------------------------- METHODS ----------------------------------
*/

int WebServer::startServer()
{
	// set up the listening socket
	mListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (mListenSocket < 0)
	{
		std::cerr << "socket() failed" << std::strerror(errno) << std::endl;
		return 1;
	}
	std::cout << "Socket created with value: " << mListenSocket << std::endl;

	// OS doesn't immediately free the port, which causes web server to hang
	// if you close and reopen it quickly. This tells our server that we can
	// reuse the port.
	int enable = 1;
	if (setsockopt(mListenSocket, SOL_SOCKET, SO_REUSEADDR, &enable,
	               sizeof(int)) < 0)
	{
		std::cerr << "setsockopt(SO_REUSEADDR) failed" << std::strerror(errno)
				  << std::endl;
	}

	// set the socket to be non-blocking
	setNonBlockingFlag(mListenSocket);

	// bind socket and sever port
	if (bind(mListenSocket, (sockaddr *)&mSocketAddress, mSocketAdddressLen) <
	    0)
	{
		std::cerr << "bind() failed" << std::strerror(errno) << std::endl;
		return 1;
	}
	std::cout << "socket bound successfully!" << std::endl;
	return 0;
}

// call this after construction to "start" the server
// currently, webserver starts listening and essentially "waits" until it hears
// a request. Then it grabs the request (no parsing yet) and sends some data
// back (test.html for now). Then it shuts down.
void WebServer::startListen()
{
	if (listen(mListenSocket, 8) < 0)
	{
		std::cerr << "listen() failed" << std::strerror(errno) << std::endl;
		return;
	}
	std::cout << "Server now listening on: " << mIpAddress
			  << ", port: " << mPort << std::endl;

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

// accepts the client connection
// m_newSocket is the new socket established between client and server
// m_socket will be used to listen to further connections
// so far, simply reads and prints the client request
void WebServer::acceptConnection()
{
	int newSocket = 0;

	// TODO: seems like you're recycling mSocketAddress... is the info in there
	// relevant? Do you need to malloc it for each individual client
	newSocket =
		accept(mListenSocket, (sockaddr *)&mSocketAddress, &mSocketAdddressLen);
	if (newSocket < 0)
	{
		std::cerr << "accept() failed" << std::strerror(errno) << std::endl;
	}
	std::cout << "accepted client, newSocket fd: " << newSocket << std::endl;
	std::cout << "client ip address: "
			  << int(mSocketAddress.sin_addr.s_addr & 0xFF) << "."
			  << int((mSocketAddress.sin_addr.s_addr & 0xFF00) >> 8) << "."
			  << int((mSocketAddress.sin_addr.s_addr & 0xFF0000) >> 16) << "."
			  << int((mSocketAddress.sin_addr.s_addr & 0xFF000000) >> 24)
			  << std::endl;

	// update the pollfd struct
	struct pollfd newClient = {newSocket, POLLIN};
	mPollFdVector.push_back(newClient);

	// update clientState with new client
	ClientState state;
	state.bytesRead = 0;
	state.bytesSent = 0;
	state.fd = newSocket;
	mClients[newSocket] = state;
}

void WebServer::parseRequest(int clientNum)
{
	ClientState &client = mClients[mPollFdVector[clientNum].fd];
	char buffer[4096] = {0};

	client.bytesRead = recv(mPollFdVector[clientNum].fd, buffer, 4096, 0);
	client.request += buffer;
	if (client.bytesRead < 0)
	{
		std::cerr << "could not read client request" << std::endl;
	}
	else if (client.bytesRead == 0)
	{
		// request of zero bytes received, shut down the connection
		// TODO: other conditions and revents can also require us to close the
		// connection; turn this into a function, find all other applicable
		// revents
		std::cerr << "recv(): zero bytes received, connection "
				  << mPollFdVector[clientNum].fd << " closed" << std::endl;
		close(mPollFdVector[clientNum].fd);
		mClients.erase(mPollFdVector[clientNum].fd);
		mPollFdVector.erase(mPollFdVector.begin() + clientNum);
	}
	else if (client.request.find("\r\n\r\n") != std::string::npos)
	{
		// std::cout << std::endl
		// 		  << "***** client request *****" << std::endl
		// 		  << std::endl;
		// std::cout << client.request;
		// std::cout << "***** client request over *****" << std::endl
		// 		  << std::endl;
		mPollFdVector[clientNum].events |= POLLOUT;
		generateResponse(mPollFdVector[clientNum].fd);
		client.request.erase();
	}
}

// called at construction, creates the response message
// ifstream sets input stream to a file, ostringstream reads the entire file
// using rdbuf() body is the html file contents, response is some information
// plus the body
// change test.html to point to a default page of some kind
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

void WebServer::generateResponse(int clientFd)
{
	ClientState &client = mClients[clientFd];
	ssize_t bytes = 0;
	// CGI demo code, currently overwriting the default response message
	// to determine if a cgi script needs to be run or not, we need to check if
	// the requested file ends in .py etc, or if the requested file is in the
	// cgi-bin folder
	int isCGI = 1;
	if (isCGI)
	{
		ResponseBuilder dummyObj = ResponseBuilder();
		CgiHandler cgiObj = CgiHandler();

		// executes a simple python script, here we would pass a container or
		// something instead of a hardcoded string
		cgiObj.execute("hello.py");
		client.response = dummyObj.buildCgiResponse(cgiObj.getOutFd());
	}
	else
	{
		client.response = defaultResponse();
	}

	// std::cout << " ***** Server response ***** " << std::endl;
	// std::cout << client.response << std::endl;
	// std::cout << " ***** Server response over ***** " << std::endl;
}

// send data to client
bool WebServer::sendResponse(int clientFd)
{
	ClientState &client = mClients[clientFd];
	ssize_t bytes = 0;

	// send everything in m_serverResponse to client
	std::cout << "attempting to send response to client" << std::endl;
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
		std::cerr << "send() failed" << std::endl;
	}
	else
	{
		std::cout << "send() success!" << std::endl;
		return true;
	}
	return false;
}

void WebServer::closeServer() const
{
	std::signal(SIGINT, SIG_DFL);
	close(mListenSocket);
	for (int i = 0; i < mPollFdVector.size(); i++)
		close(mPollFdVector[i].fd);
	std::cout << "destructor called, web server shutting down" << std::endl;
}

/*
** --------------------------------- ACCESSOR ---------------------------------
*/

/* ************************************************************************** */

// Need to decide if this function should be a private function for this class
// or perhaps place it in a utils.cpp file for better readability
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

// signal handler function
void signalHandler(int sig)
{
	gSignal = sig;
}
