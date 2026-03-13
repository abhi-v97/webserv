#include <csignal>
#include <cstring>
#include <sys/socket.h>

#include "CgiHandler.hpp"
#include "ClientHandler.hpp"
#include "Dispatcher.hpp"
#include "IHandler.hpp"
#include "Logger.hpp"
#include "RequestManager.hpp"
#include "configParser.hpp"

#define MAX_LISTEN_REQUESTS 8

/** \var gSignal
	Global variable used to store signal information
*/
int gSignal = 0;

Dispatcher::Dispatcher(): mRequestManager(this)
{
}

Dispatcher::~Dispatcher()
{
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

/**
	\brief Main event loop for the web server

	Checks poll until an event is fired. Poll is timed out and sent to sleep for 60 seconds to
   prevent busy-wait. Every 60 sesconds, the loop checks if any inactive sessions and goes back to
   sleep if no fd was ready.
*/
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
				// inactive for 15 minutes or more
				if (timeNow - it->second->lastAccessed > 900)
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
			{
				mHandler[mPollFds[i].fd]->handleEvents(mPollFds[i]);
			}
			if (mHandler[mPollFds[i].fd]->getKeepAlive() == false)
				removeHandler(i);
		}
	}
}

/**
	\brief factory method to create a Listener handler for each port

	If the bind was successful (might fail if port is already in use), adds the created object to
	map mHandler
	\param ip IP address of the server block
	\param port port where new connections will be observed
	\param srv ServerConfig object of the server block tied to the port
*/
void Dispatcher::createListener(const std::string &ip, int port, ServerConfig srv)
{
	IHandler *listener = new Listener(ip, port, srv, this);

	if (static_cast<Listener *>(listener)->bindPort() == true)
		mHandler[listener->getFd()] = listener;
	else
		delete listener;
}

/**
	\brief factory method to add a new client to poll loop

	\param socketFd Socket FD of the newly created connection
	\param srv ServerConfig class object relating to the server block of the requested connection
	\param listener Listener object where the new connection was requested, holds connection info
   like IP address
*/
void Dispatcher::createClientHandler(int socketFd, ServerConfig *srv, Listener *listener)
{
	IHandler *client = new ClientHandler(socketFd, srv, listener, this);

	if (!client)
		return;
	mHandler[socketFd] = client;
	struct pollfd pollFdStruct = {socketFd, POLLIN, 0};
	mPollFds.push_back(pollFdStruct);
}

/**
	\brief Factory method to create a CGI handler object and add it to the poll loop

	\param client ClientHandler which requested the CGI object, needs to be configured to catch
   timeouts
   \param route RouteResult struct which contains information about the request
*/
void Dispatcher::createCgiHandler(ClientHandler *client, RouteResult &route)
{
	IHandler *cgi = new CgiHandler(client);

	if (static_cast<CgiHandler *>(cgi)->execute(route) == false)
	{
		delete cgi;
		return;
	}
	if (route.type == RR_CGI_POST)
	{
		int cgiInFd = static_cast<CgiHandler *>(cgi)->getInFd();

		mHandler[cgiInFd] = cgi;
		struct pollfd pollInFdStruct = {cgiInFd, POLLOUT, 0};
		mPollFds.push_back(pollInFdStruct);
	}

	int cgiOutFd = static_cast<CgiHandler *>(cgi)->getOutFd();
	mHandler[cgiOutFd] = cgi;
	struct pollfd pollFdStruct = {cgiOutFd, POLLIN, 0};
	client->setCgiFd(cgiOutFd);
	mPollFds.push_back(pollFdStruct);
}

/**
	\brief Used to initalise Listeners for each valid port found in the config file
*/
bool Dispatcher::setListeners()
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
		if (listen((*it).first, 1000) < 0)
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

/**
	\brief Remove a handler from the poll loop

	\param pollNum The index of the poll struct in the poll array

	Uses the poll index rather than the struct directly to preserve the ordering and prevent invalid
   indices or segfaults.
   If the handler is CgiHandler, performs some additional checks if two FDs have been registered,
   which can be the case for a CGI POST request
*/
void Dispatcher::removeHandler(int &pollNum)
{
	int handlerFd = mPollFds[pollNum].fd;

	if (dynamic_cast<CgiHandler *>(mHandler[handlerFd]))
	{
		LOG_INFO(std::string("closing CGI handler: ") + numToString(handlerFd));
		std::map<int, IHandler *>::iterator mit = mHandler.find(handlerFd);

		int cgiIn = dynamic_cast<CgiHandler *>(mHandler[handlerFd])->getInFd();
		int cgiOut = dynamic_cast<CgiHandler *>(mHandler[handlerFd])->getOutFd();
		int pollSize = mPollFds.size();
		for (int i = pollSize - 1; i >= 0; i--)
		{
			std::map<int, IHandler *>::iterator it = mHandler.find(mPollFds[i].fd);
			if (it != mHandler.end() && it->second == mit->second)
			{
				mPollFds.erase(mPollFds.begin() + i);
			}
		}
		delete mHandler[handlerFd];
		mHandler.erase(cgiIn);
		mHandler.erase(cgiOut);
		pollNum = -1;
		return;
	}
	LOG_NOTICE(std::string("closing connection ") + numToString(handlerFd));
	delete mHandler[handlerFd];
	mHandler.erase(handlerFd);
	mPollFds.erase(mPollFds.begin() + pollNum);
	close(handlerFd);
}

/**
	\brief Called by ClientHanlder when a new Session needs to be monitored

	\param sessionId Randomly generated session ID string
*/
Session *Dispatcher::addSession(const std::string &sessionId)
{
	Session *newSession = new Session();
	time_t	 now = time(NULL);

	newSession->lastAccessed = now;
	newSession->timeCreated = now;
	newSession->user = "";
	mSessions[sessionId] = newSession;
	return (newSession);
}

/**
	\brief getter for a session object based on the session Id string

	\param sessionId string representation of the session ID
*/
Session *Dispatcher::getSession(const std::string &sessionId)
{
	return (this->mSessions[sessionId]);
}

/**
	\brief Called by ClientHandler when a CGI entity has timed out and needs to be forcefully shut
   down

   \param cgiFd The polled fd of the CgiHandler object
*/
void Dispatcher::closeCgi(int cgiFd)
{
	mHandler[cgiFd]->mKeepAlive = false;
}

/**
	\brief Called by ClientHandler when an expired session has been detected

	\param sessionId The id of the session to be deleted
*/
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

/**
	\brief Returns an instance of RequestManager to ClientHandler to handle a fully parsed request
*/
RequestManager &Dispatcher::getRouter()
{
	return (mRequestManager);
}
