#include <cerrno>
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

// TODO: the formatter insists on rearranging header files to be in ascending
// orde
/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

WebServer::WebServer()
	: mPort(), mListenSocket(), mClientCount(0), mSocketAddress(),
	  mSocketAdddressLen(), mPollFdStruct()
{
}

WebServer::WebServer(const WebServer &src)
	: mPort(), mListenSocket(), mClientCount(0), mSocketAddress(),
	  mSocketAdddressLen(), mPollFdStruct()
{
	(void)src;
}

WebServer::WebServer(std::string ipAddress, int port)
	: mIpAddress(ipAddress), mPort(port), mListenSocket(), mClientCount(0),
	  mSocketAddress(), mSocketAdddressLen(sizeof(mSocketAddress)),
	  mPollFdStruct()
{
	mSocketAddress.sin_family = AF_INET;
	mSocketAddress.sin_port = htons(mPort);
	mSocketAddress.sin_addr.s_addr = inet_addr(mIpAddress.c_str());

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
	if (listen(mListenSocket, 1) < 0)
	{
		std::cerr << "listen() failed" << std::strerror(errno) << std::endl;
		return;
	}
	std::cout << "Server now listening on: " << mIpAddress
			  << ", port: " << mPort << std::endl;

	// add listening socket to pollfd struct so poll() can monitor it
	mPollFdStruct[0].fd = mListenSocket;
	mPollFdStruct[0].events = POLLIN;
	while (true)
	{
		int pollNum = poll(mPollFdStruct, mClientCount + 1, 1);
		for (int i = 0; i < mClientCount + 1; i++)
		{
			if (mPollFdStruct[i].fd == mListenSocket &&
			    (mPollFdStruct[i].revents & POLLIN))
			{
				// if this condition is true, it means we have a new connection
				acceptConnection();
			}
			else if (mPollFdStruct[i].revents & POLLIN)
			{
				// this means we're now dealing with a client
				parseRequest(i);
			}
			if (mPollFdStruct[i].revents & POLLOUT)
			{
				// if we have enough data, send reponse to client
				if (sendResponse(mPollFdStruct[i].fd))
				{
					// if all data has been sent, remove POLLOUT flag
					mPollFdStruct[i].events &= ~POLLOUT;
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

	if (mClientCount > MAX_CLIENTS)
	{
		std::cerr << "too many connections, for now" << std::endl;
		exit(1);
	}
	newSocket =
		accept(mListenSocket, (sockaddr *)&mSocketAddress, &mSocketAdddressLen);
	if (newSocket < 0)
	{
		std::cerr << "accept() failed" << std::strerror(errno) << std::endl;
	}
	std::cout << "accepted client, newSocket fd: " << newSocket << std::endl;

	// update the pollfd struct
	mClientCount++;
	mPollFdStruct[mClientCount].fd = newSocket;
	mPollFdStruct[mClientCount].events = POLLIN;

	// update clientState with new client
	ClientState state;
	state.bytesRead = 0;
	state.bytesSent = 0;
	state.fd = newSocket;
	mClients[newSocket] = state;
}

void WebServer::parseRequest(int clientNum)
{
	ClientState &client = mClients[mPollFdStruct[clientNum].fd];
	char buffer[4096] = {0};

	client.bytesRead = recv(mPollFdStruct[clientNum].fd, buffer, 4096, 0);
	client.request += buffer;
	if (client.bytesRead < 0)
	{
		std::cerr << "could not read client request" << std::endl;
	}
	else if (client.bytesRead == 0)
	{
		std::cerr << "recv(): zero bytes received, connection "
				  << mPollFdStruct[clientNum].fd << " closed" << std::endl;
		// remove the client from pollfd struct by overwriting it
		// with the last item
		mClients.erase(mPollFdStruct[clientNum].fd);
		close(mPollFdStruct[clientNum].fd);
		mPollFdStruct[clientNum] = mPollFdStruct[mClientCount];
		mClientCount--;
	}
	else if (client.request.find("\r\n\r\n") != std::string::npos)
	{
		std::cout << std::endl
				  << "***** client request *****" << std::endl
				  << std::endl;
		std::cout << client.request;
		std::cout << "***** client request over *****" << std::endl
				  << std::endl;
		mPollFdStruct[clientNum].events |= POLLOUT;
		generateResponse(mPollFdStruct[clientNum].fd);
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

	std::cout << " ***** Server response ***** " << std::endl;
	std::cout << client.response << std::endl;
	std::cout << " ***** Server response over ***** " << std::endl;
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
			break;
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
	close(mListenSocket);
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
