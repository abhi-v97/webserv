#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h> // used for fcntl(), believe it or not
#include <fstream>
#include <iostream>
#include <poll.h> // used for poll()
#include <sstream>
#include <sys/poll.h>
#include <unistd.h>

#include "CgiHandler.hpp"
#include "ResponseBuilder.hpp"
#include "WebServer.hpp"

// used to configure poll()
#define MAX_CLIENTS 10

/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

WebServer::WebServer()
	: m_port(), m_socket(), m_newSocket(), m_clientCount(0),
	  m_incomingMessage(), m_socketAddress(), m_socketAdddress_len()
{
}

WebServer::WebServer(const WebServer &src)
	: m_port(), m_socket(), m_newSocket(), m_clientCount(0),
	  m_incomingMessage(), m_socketAddress(), m_socketAdddress_len()
{
	(void)src;
}

WebServer::WebServer(std::string ipAddress, int port)
	: m_ipAddress(ipAddress), m_port(port), m_socket(), m_newSocket(),
	  m_clientCount(0), m_incomingMessage(), m_socketAddress(),
	  m_socketAdddress_len(sizeof(m_socketAddress))
{
	m_socketAddress.sin_family = AF_INET;
	m_socketAddress.sin_port = htons(m_port);
	m_socketAddress.sin_addr.s_addr = inet_addr(m_ipAddress.c_str());

	if (startServer() > 0)
	{
		std::cerr << "cannot connect to socket: " << m_port << ": "
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

WebServer &WebServer::operator=(const WebServer &rhs)
{
	(void)rhs;
	return *this;
}

std::ostream &operator<<(std::ostream &outf, const WebServer &obj)
{
	// o << "Value = " << i.getValue();
	(void)obj;
	(void)outf;
	return outf;
}

/*
** --------------------------------- METHODS ----------------------------------
*/

int WebServer::startServer()
{
	// set up the listening socket
	m_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_socket < 0)
	{
		std::cerr << "socket() failed" << std::strerror(errno) << std::endl;
		return 1;
	}
	std::cout << "Socket created with value: " << m_socket << std::endl;

	// OS doesn't immediately free the port, which causes web server to hang
	// if you close and reopen it quickly. This tells our server that we can
	// reuse the port.
	int enable = 1;
	if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) <
	    0)
	{
		std::cerr << "setsockopt(SO_REUSEADDR) failed" << std::strerror(errno)
				  << std::endl;
	}

	// set the socket to be non-blocking
	setNonBlockingFlag(m_socket);

	// bind socket and sever port
	if (bind(m_socket, (sockaddr *)&m_socketAddress, m_socketAdddress_len) < 0)
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
	if (listen(m_socket, 1) < 0)
	{
		std::cerr << "listen() failed" << std::strerror(errno) << std::endl;
		return;
	}
	std::cout << "Server now listening on: " << m_ipAddress
			  << ", port: " << m_port << std::endl;

	int pollFd;
	struct pollfd pollFds[MAX_CLIENTS];

	pollFds[0].fd = m_socket;
	pollFds[0].events = POLLIN;
	while (true)
	{
		int pollNum = poll(pollFds, m_clientCount + 1, 1);
		for (int i = 0; i < m_clientCount + 1; i++)
		{
			if (pollFds[i].fd == m_socket && (pollFds[i].revents & POLLIN))
			{
				// if this condition is true, it means we have a new connection
				// that we need to set up
				acceptConnection(pollFds);
			}
			else if (pollFds[i].revents & POLLIN)
			{
				// else, this means we're now dealing with a client
				ssize_t bytes = 0;
				char buffer[4096] = {0};

				bytes = recv(pollFds[i].fd, buffer, 4096, 0);
				if (bytes < 0)
				{
					std::cerr << "could not read client request" << std::endl;
				}
				else if (bytes == 0)
				{
					std::cerr << "recv(): zero bytes received, connection "
							  << pollFds[i].fd << " closed" << std::endl;
				}
				else
				{
					std::cout << std::endl
							  << "***** client request *****" << std::endl
							  << std::endl;
					std::cout << buffer;
					std::cout << "***** client request over *****" << std::endl
							  << std::endl;
					pollFds[i].events |= POLLOUT;
				}
			}
			if (pollFds[i].revents & POLLOUT)
			{
				// we have enough data, now send reponse to client
				if (sendResponse(pollFds[i].fd))
				{
					// if all data has been sent, remove POLLOUT flag
					pollFds[i].events &= ~POLLOUT;
				}
			}
		}
	}
}

// accepts the client connection
// m_newSocket is the new socket established between client and server
// m_socket will be used to listen to further connections
// so far, simply reads and prints the client request
void WebServer::acceptConnection(struct pollfd *pollFds)
{
	if (m_clientCount > MAX_CLIENTS)
	{
		std::cerr << "too many connections, for now" << std::endl;
		exit(1);
	}
	m_newSocket =
		accept(m_socket, (sockaddr *)&m_socketAddress, &m_socketAdddress_len);
	if (m_newSocket < 0)
	{
		std::cerr << "accept() failed" << std::strerror(errno) << std::endl;
	}
	std::cout << "accepted client, newSocket fd: " << m_newSocket << std::endl;
	m_clientCount++;
	pollFds[m_clientCount].fd = m_newSocket;
	pollFds[m_clientCount].events = POLLIN;
}

// called at construction, creates the response message
// ifstream sets input stream to a file, ostringstream reads the entire file
// using rdbuf() body is the html file contents, response is some information
// plus the body
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

// send data to client
bool WebServer::sendResponse(int clientFd)
{
	// CGI demo code, currently overwriting the default response message
	int bytesSent;
	int totalBytes = 0;

	// to determine if a cgi script needs to be run or not, we need to check if
	// the requested file ends in .py etc, or if the requested file is in the
	// cgi-bin folder
	int isCGI = 0;
	if (isCGI)
	{
		ResponseBuilder dummyObj = ResponseBuilder();
		CgiHandler cgiObj = CgiHandler();

		// executes a simple python script, here we would pass a container or
		// something instead of a hardcoded string
		cgiObj.execute("hello.py");
		m_serverResponse = dummyObj.buildCgiResponse(cgiObj.getOutFd());
	}
	else
	{
		m_serverResponse = defaultResponse();
	}

	std::cout << " ***** Server response ***** " << std::endl;
	std::cout << m_serverResponse << std::endl;
	std::cout << " ***** Server response over ***** " << std::endl;

	// send everything in m_serverResponse to client
	std::cout << "attempting to send response to client" << std::endl;
	while (totalBytes < m_serverResponse.size())
	{
		bytesSent = send(clientFd, m_serverResponse.c_str(),
		                 m_serverResponse.size(), 0);
		if (bytesSent < 0)
		{
			break;
		}
		totalBytes += bytesSent;
	}
	if (totalBytes != m_serverResponse.size())
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

void WebServer::closeServer()
{
	close(m_socket);
	close(m_newSocket);
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
