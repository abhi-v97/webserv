#pragma once
#include <cstddef>
#include <map>
#include <sys/poll.h>
#include <vector>

#include "Connection.hpp"

class Reactor
{
public:
	Reactor();
	~Reactor();

	// keep listeners (front of pollfds) and connections map
	bool setListeners(const std::vector<int> &listen_fds);
	void addConnection(int listenFd); // copy (or add by value)
	void removeConnection(int fd);

	// main loop: builds pollfd array from listeners + connections and dispatches
	void loop();

private:
	size_t					  mListenCount;
	std::vector<pollfd>		  mPollFds;				// built before poll()
	std::map<int, Connection> mClients;				// key is fd
	void					  handleEvent(int idx); // dispatch for a single pollfd entry
};

void signalHandler(int sig);