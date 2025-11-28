#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>

#include "Acceptor.hpp"
#include "Utils.hpp"

#include "Logger.hpp"

Acceptor::Acceptor()
{
}

Acceptor::~Acceptor()
{
}

bool Acceptor::init(std::vector<int> ports)
{
	for (std::vector<int>::iterator it = ports.begin(); it != ports.end(); it++)
	{
		sockaddr_in socketAddress = sockaddr_in();
		socketAddress.sin_family = AF_INET;
		socketAddress.sin_port = htons(*it);
		socketAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
		if (bindPort(socketAddress) != 0)
		{
			LOG_ERROR("Failed to bind port: " + numToString(*it));
			return (false);
		}
	}
	return (true);
}

int Acceptor::createListenSocket(const sockaddr_in &addr)
{
	return (0);
}

const std::vector<int> &Acceptor::listeners() const
{
	return (this->mListeners);
}

/**
	\brief Server socket and port setup

	creates the listening socket, sets it to nonblocking, and binds the socket
	to the server port

	\param socketStruct sockaddr_in struct that contains listening port info
*/
bool Acceptor::bindPort(sockaddr_in socketStruct)
{
	// create a new socket
	int mSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (mSocket < 0)
	{
		LOG_ERROR(std::string("socket() failed: ") + std::strerror(errno));
		return (false);
	}
	LOG_NOTICE(std::string("Socket created: ") + numToString(ntohs(socketStruct.sin_port)));

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
	if (bind(mSocket, (sockaddr *) &socketStruct, sizeof(socketStruct)) < 0)
	{
		LOG_ERROR(std::string("bind() failed: ") + std::strerror(errno));
		return (false);
	}
	LOG_NOTICE(std::string("bind success: ") + numToString("127.0.0.1") + std::string(":") +
			   numToString(socketStruct.sin_port));
	mListeners.push_back(mSocket);
	return (true);
}

void Acceptor::closeAll()
{
}
