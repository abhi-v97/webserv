#pragma once
#include <cstddef>
#include <map>
#include <sys/poll.h>
#include <vector>

#include "ClientHandler.hpp"
#include "IHandler.hpp"

/**
	\class Dispatcher

	Maintains the server loop and all event handler objects currently in circulation.

	Uses the Reactor design pattern, code flow is based on this:
	https://en.wikipedia.org/wiki/Reactor_pattern#/media/File:ReactorPattern_-_UML_2_Sequence_Diagram.svg
*/
class Dispatcher
{
public:
	Dispatcher();
	~Dispatcher();

	bool setListeners();
	void createListener(int port);
	void createClient(int listenFd);
	void createCgiHandler(ClientHandler *client);
	void removeClient(int fd);

	void loop();

private:
	size_t					  mListenCount;
	std::vector<pollfd>		  mPollFds;
	std::map<int, IHandler *> mHandler;
};

void signalHandler(int sig);
