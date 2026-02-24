#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>

#include "Dispatcher.hpp"
#include "Listener.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "configParser.hpp"

Listener::Listener(int port, ServerConfig srv, Dispatcher *dispatch)
{
	mDispatch = dispatch;
	mSocketAddress = sockaddr_in();
	mSocketAddress.sin_family = AF_INET;
	mSocketAddress.sin_port = htons(port);
	mSocketAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
	mConfig = srv;
}

Listener::~Listener()
{
}

// TODO: handle requestClose for Listener events
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

int Listener::getFd() const
{
	return (this->mSocketFd);
}

bool Listener::getKeepAlive() const
{
	return (this->mKeepAlive);
}

const std::string &Listener::getIp() const
{
	return (this->mClientIp);
}