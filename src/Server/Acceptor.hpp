#pragma once

#include <netinet/in.h>

#include "IHandler.hpp"
#include "configParser.hpp"

class Dispatcher;

/**
	\class Acceptor

	Concrete class derived from IHandler.

	Responsible for creating new listen sockets for each port specified in config file.
	handleEvents() is triggered when a new connection is requested by a client on the object's port,
	results in a new client connection added to Reactor loop.
*/
class Acceptor: public IHandler
{
public:
	Acceptor(int port, ServerConfig srv, Dispatcher *dispatch);
	~Acceptor();

	void handleEvents(struct pollfd &pollStruct);
	int	 getFd() const;
	bool getKeepAlive() const;
	const std::string &getIp() const;
	bool bindPort();

private:
	std::string	 mClientIp;
	int			 mSocketFd;
	sockaddr_in	 mSocketAddress;
	Dispatcher	*mDispatch;
	ServerConfig mConfig;

	bool newConnection();
};
