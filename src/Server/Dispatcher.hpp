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

	void addHandler(IHandler *handler);
	bool setListeners();
	void removeClient(int fd);
	
	void createListener(int port);
	void createClient(int listenFd);

	// main loop: builds pollfd array from listeners + connections and dispatches
	void loop();

private:
	size_t					  mListenCount;
	std::vector<pollfd>		  mPollFds;				// built before poll()
	std::map<int, IHandler *> mHandler;				// key is fd
	void					  handleEvent(int idx); // dispatch for a single pollfd entry
};

void signalHandler(int sig);