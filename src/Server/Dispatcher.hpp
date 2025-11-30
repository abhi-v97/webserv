#pragma once
#include <cstddef>
#include <map>
#include <sys/poll.h>
#include <vector>

#include "IHandler.hpp"

class Dispatcher
{
public:
	Dispatcher();
	~Dispatcher();

	bool setListeners();
	void createListener(int port);
	void createClient(int listenFd);
	void removeClient(int fd);

	// main loop: builds pollfd array from listeners + connections and dispatches
	void loop();

private:
	size_t					  mListenCount;
	std::vector<pollfd>		  mPollFds;
	std::map<int, IHandler *> mHandler; // key = fd, value = handler object
};

void signalHandler(int sig);
