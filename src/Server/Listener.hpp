#pragma once

#include <netinet/in.h>
#include <vector>

#include "IHandler.hpp"

class Dispatcher;

class Listener: public IHandler
{
public:
	Listener();
	Listener(int port, Dispatcher *dispatch);
	~Listener();

	// bool					initServerPorts(std::vector<int> ports);
	const std::vector<int> &listeners() const;
	void					newConnection();
	void					handleEvents(struct pollfd &pollStruct);
	int						getFd() const;
	bool					bindPort();

private:
	int			mSocket;
	sockaddr_in mSocketAddress;
	Dispatcher  *mDispatch;
};
