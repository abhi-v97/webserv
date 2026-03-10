#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>

#include "Dispatcher.hpp"
#include "Listener.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "configParser.hpp"

/**
	\brief Constructor for a new listener object
	
	\param ip IP address of the server
	\param port Port address where the listen will occur
	\param srv ServerConfig object of the server block
	\param dispatch Pointer to the main dispatch loop
*/
Listener::Listener(const std::string &ip, int port, ServerConfig srv, Dispatcher *dispatch)
{
	mDispatch = dispatch;
	mSocketAddress = sockaddr_in();
	mSocketAddress.sin_family = AF_INET;
	mSocketAddress.sin_port = htons(port);
	struct in_addr ina;
	if (!ip.empty() && inet_pton(AF_INET, ip.c_str(), &ina) == 1)
		mSocketAddress.sin_addr = ina;
	else
		mSocketAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
	mConfig = srv;
}

/**
	\brief Destructor for the Listener object, closes the socket FD tied to it
*/
Listener::~Listener()
{
	LOG_INFO("Closing Listener at port: " + numToString(ntohs(mSocketAddress.sin_port)));
	close(mSocketFd);
}

/**
	\brief handles a new event on the poll loop
*/
void Listener::handleEvents(pollfd &pollStruct)
{
	if (pollStruct.revents & POLLIN)
		newConnection();
}

/**
	\brief Server socket and port setup

	creates the listening socket, sets it to nonblocking, and binds the socket
	to the server port

	\param mSocketAddress sockaddr_in struct that contains listening port info
*/
bool Listener::bindPort()
{
	// create a new socket
	mSocketFd = socket(AF_INET, SOCK_STREAM, 0);
	if (mSocketFd < 0)
	{
		LOG_ERROR(std::string("socket() failed: ") + std::strerror(errno));
		return (false);
	}

	// OS doesn't immediately free the port, which causes web server to hang
	// if you close and reopen it quickly. This tells our server that we can
	// reuse the port.
	int enable = 1;
	if (setsockopt(mSocketFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
	{
		LOG_ERROR(std::string("setsockopt failed: ") + std::strerror(errno));
		return (false);
	}

	// set the socket to be non-blocking
	setNonBlockingFlag(mSocketFd);

	// bind socket and server port
	if (bind(mSocketFd, (sockaddr *) &mSocketAddress, sizeof(mSocketAddress)) < 0)
	{
		LOG_ERROR(std::string("bind() failed: ") + std::strerror(errno));
		return (false);
	}
	LOG_NOTICE(std::string("bind(): ") + std::string("127.0.0.1:") +
			   numToString(ntohs(mSocketAddress.sin_port)));
	return (true);
}

/**
	\brief Triggered when a new connection is requested on this Listener's port

	Tries to accept a new connection, and on success registers it on the poll loop through
   Dispatcher
*/
bool Listener::newConnection()
{
	struct sockaddr_in clientAddr = {};
	socklen_t		   clientLen = sizeof(clientAddr);
	std::ostringstream ipStream;
	int				   newFd;

	newFd = accept(mSocketFd, (sockaddr *) &clientAddr, &clientLen);
	if (newFd < 0)
		return (LOG_ERROR(std::string("newConnection(): ") + std::strerror(errno)), false);

	// get client IP from sockaddr struct, store it as std::string
	ipStream << int(clientAddr.sin_addr.s_addr & 0xFF) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF00) >> 8) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF0000) >> 16) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF000000) >> 24);

	mClientIp = ipStream.str();
	LOG_INFO(std::string("newConnection(): new client fd: ") + numToString(mSocketFd) +
			 "; ip: " + mClientIp);
	mDispatch->createClientHandler(newFd, &mConfig, this);
	return (setNonBlockingFlag(newFd));
}

/**
	\brief Implemented function of IHandler, returns the FD of the Listener handler
*/
int Listener::getFd() const
{
	return (this->mSocketFd);
}

/**
	\brief Used by Dispatcher to decide if handler needs to be closed or not

	Usually true for Listener objects, will be switched to false if an error occurred or bind failed
*/
bool Listener::getKeepAlive() const
{
	return (this->mKeepAlive);
}

/**
	\brief Getter for IP address
*/
const std::string &Listener::getIp() const
{
	return (this->mClientIp);
}

/**
	\brief Getter for port address
*/
int Listener::getPort() const
{
	return (ntohs(this->mSocketAddress.sin_port));
}
