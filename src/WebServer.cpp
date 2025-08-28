#include "WebServer.hpp"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>
/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

WebServer::WebServer()
{
}

WebServer::WebServer(const WebServer &src)
{
	(void)src;
}

WebServer::WebServer(std::string ipAddress, int port)
	: m_ipAddress(ipAddress), m_port(port), m_socket(), m_newSocket(), m_incomingMessage(),
	  m_socketAddress(), m_socketAdddress_len(sizeof(m_socketAddress)),
	  m_serverResponse(getResponse())
{
	m_socketAddress.sin_family = AF_INET;
	m_socketAddress.sin_port = htons(m_port);
	m_socketAddress.sin_addr.s_addr = inet_addr(m_ipAddress.c_str());

	if (startServer() > 0)
	{
		std::cerr << "cannot connect to socket: " << m_port << ": " << std::strerror(errno)
				  << std::endl;
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

WebServer &WebServer::operator=(WebServer const &rhs)
{
	(void)rhs;
	return *this;
}

std::ostream &operator<<(std::ostream &o, WebServer const &i)
{
	// o << "Value = " << i.getValue();
	(void)o;
	return o;
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
		return (1);
	}
	std::cout << "Socket created with value: " << m_socket << std::endl;

	// OS doesn't immediately free the port, which causes web server to hang
	// if you close and reopen it quickly. This tells our server that we can
	// reuse the port.
	int enable = 1;
	if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
	{
		std::cerr << "setsockopt(SO_REUSEADDR) failed" << std::strerror(errno) << std::endl;
	}

	// bind socket and sever port
	if (bind(m_socket, (sockaddr *)&m_socketAddress, m_socketAdddress_len) < 0)
	{
		std::cerr << "bind() failed" << std::strerror(errno) << std::endl;
		return (1);
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
	std::cout << "Server now listening on: " << m_ipAddress << ", port: " << m_port << std::endl;
	acceptConnection();
	sendResponse();
}

// accepts the client connection
// m_newSocket is the new socket established between client and server
// m_socket will be used to listen to further connections
// so far, simply reads and prints the client request
void WebServer::acceptConnection()
{
	int bytes = 0;
	char buffer[4096] = {0};

	m_newSocket = accept(m_socket, (sockaddr *)&m_socketAddress, &m_socketAdddress_len);
	if (m_newSocket < 0)
		std::cerr << "accept() failed" << std::strerror(errno) << std::endl;
	std::cout << "accepted client, newSocket fd: " << m_newSocket << std::endl;
	bytes = recv(m_newSocket, buffer, 4096, 0);
	if (bytes < 0)
		std::cerr << "could not read client request" << std::endl;
	else
	{
		std::cout << std::endl << "***** client request *****" << std::endl << std::endl;
		std::cout << buffer;
		std::cout << "***** client request over *****" << std::endl << std::endl;
	}
}

// called at construction, creates the response message
// confusing, needs to be broken up into multiple functions
// ifstream sets input stream to a file, ostringstream reads the entire file using rdbuf()
// body is the html file contents, response is some information plus the body
std::string WebServer::getResponse()
{
	std::ifstream htmlFile("test.html");
	std::ostringstream body;
	std::ostringstream response;

	body << htmlFile.rdbuf();
	response << "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: " << body.str().size()
			 << "\n\n"
			 << body.str();
	return (response.str());
}

// send data to client
void WebServer::sendResponse()
{
	int bytesSent;
	int totalBytes = 0;

	std::cout << "attempting to send response to client" << std::endl;
	while (totalBytes < m_serverResponse.size())
	{
		bytesSent = send(m_newSocket, m_serverResponse.c_str(), m_serverResponse.size(), 0);
		if (bytesSent < 0)
			break;
		totalBytes += bytesSent;
	}
	if (totalBytes != m_serverResponse.size())
		std::cerr << "send() failed" << std::endl;
	else
	 	std::cout << "send() success!" << std::endl;
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