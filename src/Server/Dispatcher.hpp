#pragma once
#include <cstddef>
#include <ctime>
#include <map>
#include <string>
#include <sys/poll.h>
#include <vector>

#include "ClientHandler.hpp"
#include "IHandler.hpp"
#include "configParser.hpp"

struct Session
{
	std::string user;
	time_t		timeCreated;
	time_t		lastAccessed;
};

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
	void createListener(int port, ServerConfig srv);
	void createClient(int listenFd, ServerConfig *srv);
	void createCgiHandler(ClientHandler *client);
	void removeClient(int fd);
	Session *addSession(std::string sessionId);
	Session *getSession(std::string sessionId);
	void deleteSession(std::string &sessionId);

	void loop();

private:
	size_t							 mListenCount;
	std::vector<pollfd>				 mPollFds;
	std::map<int, IHandler *>		 mHandler;
	std::map<std::string, Session *> mSessions;
};

void signalHandler(int sig);
