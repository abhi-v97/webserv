#include <csignal>
#include <sys/socket.h>

#include "CgiHandler.hpp"
#include "WriteHandler.hpp"
#include "Dispatcher.hpp"
#include "IHandler.hpp"
#include "Acceptor.hpp"
#include "Logger.hpp"
#include "configParser.hpp"

#define MAX_LISTEN_REQUESTS 8

/** \var gSignal
	Global variable used to store signal information
*/
int gSignal = 0;

Dispatcher::Dispatcher()
{
}

Dispatcher::~Dispatcher()
{
	LOG_NOTICE("destructor called, web server shutting down");
	std::signal(SIGINT, SIG_DFL);
	for (int i = 0; i < mPollFds.size(); i++)
	{
		delete mHandler[mPollFds[i].fd];
		close(mPollFds[i].fd);
	}
	for (std::map<std::string, Session *>::iterator it = mSessions.begin(); it != mSessions.end();
		 it++)
	{
		if (it->second)
			delete it->second;
	}
	mSessions.clear();
}

void Dispatcher::loop()
{
	std::signal(SIGINT, SIG_IGN);
	std::signal(SIGINT, signalHandler);
	time_t nextReap = time(NULL) + 900;

	while (gSignal == 0)
	{
		time_t timeNow = time(NULL);
		int	   timeout = 60; // 60s timeout cap
		long   remaining = static_cast<long>(nextReap - timeNow);

		if (remaining < 1)
			timeout = 1;
		else if (remaining < timeout)
			timeout = static_cast<int>(remaining);

		int pollNum = poll(mPollFds.data(), static_cast<int>(mPollFds.size()), timeout * 1000);
		if (pollNum < 0)
		{
			if (gSignal != SIGINT)
				LOG_FATAL(std::string("loop(): poll() failed: ") + std::strerror(errno));
			return;
		}
		// reaper, delete inactive sessions every 15 minutes (900 secs)
		timeNow += timeout;
		if (timeNow >= nextReap)
		{
			for (std::map<std::string, Session *>::iterator it = mSessions.begin();
				 it != mSessions.end();
				 it++)
			{
				// inactive for 30 minutes or more
				if (timeNow - it->second->lastAccessed > 1800)
				{
					delete it->second;
					mSessions.erase(it);
				}
			}
			nextReap = timeNow + 900;
		}
		// poll timed out, if no fds were ready, continue to next poll
		if (pollNum == 0)
			continue;
		// web server main event loop
		for (int i = static_cast<int>(mPollFds.size()) - 1; i >= 0; i--)
		{
			if (mPollFds[i].revents != 0)
				mHandler[mPollFds[i].fd]->handleEvents(mPollFds[i]);
			if (mHandler[mPollFds[i].fd]->getKeepAlive() == false)
				removeClient(i);
		}
	}
}

/**
	\brief factory method to create a acceptor handler for each port

	If the bind was successful (might fail if port is already in use), adds the created object to
	map mHandler
*/
void Dispatcher::createAcceptor(int port, ServerConfig srv)
{
	IHandler *acceptor = new Acceptor(port, srv, this);

	if (static_cast<Acceptor *>(acceptor)->bindPort() == true)
		mHandler[acceptor->getFd()] = acceptor;
	else
		delete acceptor;
}

/**
	\brief factory method to add a new client to poll loop
*/
void Dispatcher::createReadHandler(int socketFd, ServerConfig *srv, Acceptor *acceptor)
{
	IHandler *client = new ReadHandler(socketFd, srv, acceptor, this);

	if (!client)
		return;
	mHandler[socketFd] = client;
	struct pollfd pollFdStruct = {socketFd, POLLIN, 0};
	mPollFds.push_back(pollFdStruct);
}

void Dispatcher::createWriteHandler(int socketFd, RequestParser &requestObj, ServerConfig *srv)
{
	IHandler *client = new WriteHandler(socketFd, requestObj, srv);

	if (!client)
		return;
	mHandler[socketFd] = client;
	struct pollfd pollFdStruct = {socketFd, POLLIN, 0};
	mPollFds.push_back(pollFdStruct);
}

void Dispatcher::createCgiHandler(WriteHandler *client)
{
	IHandler *cgi = new CgiHandler(client);

	if (static_cast<CgiHandler *>(cgi)->execute("hello.py") == false)
	{
		delete cgi;
		return;
	}
	int cgiFd = cgi->getFd();

	mHandler[cgiFd] = cgi;
	struct pollfd pollFdStruct = {cgiFd, POLLIN, 0};
	client->setCgiFd(cgiFd);
	mPollFds.push_back(pollFdStruct);
}

bool Dispatcher::setAcceptors()
{
	mListenCount = mHandler.size();
	if (mListenCount == 0)
	{
		LOG_FATAL("No ports bound, shutting down");
		return (false);
	}
	mPollFds.reserve(mListenCount);
	for (std::map<int, IHandler *>::const_iterator it = mHandler.begin(); it != mHandler.end();
		 it++)
	{
		if (listen((*it).first, MAX_LISTEN_REQUESTS) < 0)
		{
			LOG_ERROR(std::string("listen() failed: ") + std::strerror(errno));
			return (false);
		}
		struct pollfd listenStruct = {(*it).first, POLLIN, 0};
		mPollFds.push_back(listenStruct);
	}
	LOG_INFO("Server ready; listening for requests");
	return (true);
}

void Dispatcher::removeClient(int pollNum)
{
	int clientFd = mPollFds[pollNum].fd;

	LOG_NOTICE(std::string("closing connection ") + numToString(clientFd));
	delete mHandler[clientFd];
	mHandler.erase(clientFd);
	mPollFds.erase(mPollFds.begin() + pollNum);
	close(clientFd);
}

Session *Dispatcher::addSession(std::string sessionId)
{
	Session *newSession = new Session();
	time_t	 now = time(NULL);

	newSession->lastAccessed = now;
	newSession->timeCreated = now;
	newSession->user = "";
	mSessions[sessionId] = newSession;
	return (newSession);
}

Session *Dispatcher::getSession(const std::string &sessionId)
{
	return (this->mSessions[sessionId]);
}

void Dispatcher::deleteSession(const std::string &sessionId)
{
	delete mSessions[sessionId];
	mSessions.erase(sessionId);
}

/**
	Helper function which changes the global signal variable when a signal is
	received

	\param sig signal value
*/
void signalHandler(int sig)
{
	gSignal = sig;
}
