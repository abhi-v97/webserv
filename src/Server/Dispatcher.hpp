#pragma once

#include <cstddef>
#include <ctime>
#include <map>
#include <string>
#include <sys/poll.h>
#include <vector>

#include "Listener.hpp"
#include "ClientHandler.hpp"
#include "IHandler.hpp"
#include "RequestManager.hpp"
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

	bool		   setListeners();
	void		   createListener(int port, ServerConfig srv);
	void		   createClientHandler(int socketFd, ServerConfig *srv, Listener *listener);
	void		   createCgiHandler(ClientHandler *client);
	void		   removeClient(int fd);
	Session		  *addSession(std::string sessionId);
	Session		  *getSession(const std::string &sessionId);
	void		   deleteSession(const std::string &sessionId);
	RequestManager &getRouter();

	void loop();

private:
	size_t							 mListenCount;
	std::vector<pollfd>				 mPollFds;
	std::map<int, IHandler *>		 mHandler;
	std::map<std::string, Session *> mSessions;
	RequestManager					 mRequestManager;
};

void signalHandler(int sig);
