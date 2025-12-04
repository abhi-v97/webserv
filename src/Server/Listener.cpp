#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>

#include "Dispatcher.hpp"
#include "Listener.hpp"
#include "Logger.hpp"
#include "Utils.hpp"

Listener::Listener()
{
}

Listener::~Listener()
{
}

Listener::Listener(int port, Dispatcher *dispatch)
{
	mDispatch = dispatch;
	mSocketAddress = sockaddr_in();
	mSocketAddress.sin_family = AF_INET;
	mSocketAddress.sin_port = htons(port);
	mSocketAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
}

void Listener::handleEvents(pollfd &pollStruct)
{
	if (pollStruct.revents & POLLIN)
		mDispatch->createClient(mSocket);
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
	mSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (mSocket < 0)
	{
		LOG_ERROR(std::string("socket() failed: ") + std::strerror(errno));
		return (false);
	}

	// OS doesn't immediately free the port, which causes web server to hang
	// if you close and reopen it quickly. This tells our server that we can
	// reuse the port.
	int enable = 1;
	if (setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
	{
		LOG_ERROR(std::string("setsockopt failed: ") + std::strerror(errno));
		return (false);
	}

	// set the socket to be non-blocking
	setNonBlockingFlag(mSocket);

	// bind socket and server port
	if (bind(mSocket, (sockaddr *) &mSocketAddress, sizeof(mSocketAddress)) < 0)
	{
		LOG_ERROR(std::string("bind() failed: ") + std::strerror(errno));
		return (false);
	}
	LOG_NOTICE(std::string("bind(): ") + std::string("127.0.0.1:") +
			   numToString(ntohs(mSocketAddress.sin_port)));
	return (true);
}

int Listener::getFd() const
{
	return (this->mSocket);
}

bool Listener::getKeepAlive() const
{
	return (this->mKeepAlive);
}

