#include <csignal>
#include <sys/socket.h>

#include "CgiHandler.hpp"
#include "ClientHandler.hpp"
#include "Dispatcher.hpp"
#include "IHandler.hpp"
#include "Listener.hpp"
#include "Logger.hpp"

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
}

void Dispatcher::loop()
{
	std::signal(SIGINT, SIG_IGN);
	std::signal(SIGINT, signalHandler);
	while (gSignal == 0)
	{
		int pollNum = poll(mPollFds.data(), static_cast<int>(mPollFds.size()), 1);
		if (pollNum < 0)
		{
			LOG_FATAL(std::string("poll() failed: ") + std::strerror(errno));
		}
		for (int i = static_cast<int>(mPollFds.size()) - 1; i >= 0; i--)
		{
			if (mPollFds[i].revents != 0)
				mHandler[mPollFds[i].fd]->handleEvents(mPollFds[i]);
			if (mHandler[mPollFds[i].fd]->getKeepAlive() == false)
			{
				removeClient(i);
			}
		}
	}
}

/**
	\brief factory method to create a listener handler for each port

	If the bind was successful (might fail if port is already in use), adds the created object to
	map mHandler
*/
void Dispatcher::createListener(int port)
{
	IHandler *listener = new Listener(port, this);

	if (static_cast<Listener *>(listener)->bindPort() == true)
		mHandler[listener->getFd()] = listener;
	else
		delete listener;
}

/**
	\brief factory method to add a new client to poll loop
*/
void Dispatcher::createClient(int listenFd)
{
	IHandler *client = new ClientHandler();

	if (static_cast<ClientHandler *>(client)->acceptSocket(listenFd, this) == false)
	{
		delete client;
		return;
	}
	mHandler[client->getFd()] = client;
	struct pollfd pollFdStruct = {client->getFd(), POLLIN, 0};
	mPollFds.push_back(pollFdStruct);
}

void Dispatcher::createCgiHandler(ClientHandler *client)
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

void Dispatcher::removeClient(int clientFd)
{
	LOG_NOTICE(std::string("closing connection ") + numToString(mPollFds[clientFd].fd));
	delete mHandler[mPollFds[clientFd].fd];
	mHandler.erase(mPollFds[clientFd].fd);
	mPollFds.erase(mPollFds.begin() + clientFd);
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
