#pragma once

#include <netinet/in.h>

#include "IHandler.hpp"

class Dispatcher;

/**
	\class Listener

	Concrete class derived from IHandler.

	Responsible for creating new listener sockets for each port specified in config file.
	handleEvents() is triggered when a new connection is requested by a client on the object's port,
	results in a new client connection added to Reactor loop.
*/
class Listener: public IHandler
{
public:
	Listener();
	Listener(int port, Dispatcher *dispatch);
	~Listener();

	void newConnection();
	void handleEvents(struct pollfd &pollStruct);
	int	 getFd() const;
	bool getKeepAlive() const;
	bool bindPort();

private:
	int			mSocket;
	sockaddr_in mSocketAddress;
	Dispatcher *mDispatch;
};
