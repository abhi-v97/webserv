#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>

#include "Acceptor.hpp"
#include "Connection.hpp"
#include "Logger.hpp"
#include "Utils.hpp"

Acceptor::Acceptor()
{
}

Acceptor::~Acceptor()
{
}

bool Acceptor::initServerPorts(std::vector<int> ports)
{
	for (std::vector<int>::iterator it = ports.begin(); it != ports.end(); it++)
	{
		sockaddr_in socketAddress = sockaddr_in();
		socketAddress.sin_family = AF_INET;
		socketAddress.sin_port = htons(*it);
		socketAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
		if (bindPort(socketAddress) != true)
		{
			std::cerr << std::strerror(errno) << std::endl;
			LOG_ERROR("Failed to bind port: " + numToString(*it));
			return (false);
		}
	}
	return (true);
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
	LOG_NOTICE(std::string("Socket created: ") + numToString(mSocket));

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
	LOG_NOTICE(std::string("bind success: ") + std::string("127.0.0.1: ") +
			   numToString(ntohs(socketStruct.sin_port)));
	mListeners.push_back(mSocket);
	return (true);
}

void Acceptor::newConnection(int listenFd)
{
	int				   newSocket;
	struct sockaddr_in clientAddr = {};
	socklen_t		   clientLen = sizeof(clientAddr);

	newSocket = accept(listenFd, (sockaddr *) &clientAddr, &clientLen);
	if (newSocket < 0)
		LOG_ERROR(std::string("accept() failed: ") + std::strerror(errno));
	LOG_NOTICE(std::string("accepted client with fd: ") + numToString(newSocket));

	// get client IP from sockaddr struct, store it as std::string
	std::ostringstream clientIp;
	clientIp << int(clientAddr.sin_addr.s_addr & 0xFF) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF00) >> 8) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF0000) >> 16) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF000000) >> 24) << std::endl;

	std::cout << "TEST: clientIp: " << clientIp.str() << std::endl;
}

const std::vector<int> &Acceptor::listeners() const
{
	return (this->mListeners);
}

void Acceptor::closeAll()
{
}
