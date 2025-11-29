#include <csignal>
#include <netinet/in.h>
#include <sys/socket.h>

#include "Connection.hpp"
#include "Logger.hpp"
#include "Reactor.hpp"
#include "Utils.hpp"

#define MAX_LISTEN_REQUESTS 8

/** \var gSignal
	Global variable used to store signal information
*/
int gSignal = 0;

Reactor::Reactor()
{
}

Reactor::~Reactor()
{
	LOG_NOTICE("destructor called, web server shutting down");
	std::signal(SIGINT, SIG_DFL);
	for (int i = 0; i < mPollFds.size(); i++)
	{
		close(mPollFds[i].fd);
	}
}

bool Reactor::setListeners(const std::vector<int> &listenFds)
{
	mListenCount = listenFds.size();
	if (mListenCount == 0)
	{
		LOG_FATAL("No ports bound, server shutting down");
		return (false);
	}
	mPollFds.reserve(mListenCount);
	for (std::vector<int>::const_iterator it = listenFds.begin(); it != listenFds.end(); it++)
	{
		if (listen(*it, MAX_LISTEN_REQUESTS) < 0)
		{
			LOG_ERROR(std::string("listen() failed: ") + std::strerror(errno));
			return (false);
		}
		struct pollfd listenStruct = {*it, POLLIN, 0};
		mPollFds.push_back(listenStruct);
	}
	return (true);
}

void Reactor::addConnection(int listenFd)
{
	int				   newSocket;
	struct sockaddr_in clientAddr = {};
	socklen_t		   clientLen = sizeof(clientAddr);

	newSocket = accept(listenFd, (sockaddr *) &clientAddr, &clientLen);
	if (newSocket < 0)
		LOG_ERROR(std::string("accept() failed: ") + std::strerror(errno));
	LOG_NOTICE(std::string("accepted client with fd: ") + numToString(newSocket));

	// get client IP from sockaddr struct, store it as std::string
	std::ostringstream clientIp;
	clientIp << int(clientAddr.sin_addr.s_addr & 0xFF) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF00) >> 8) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF0000) >> 16) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF000000) >> 24) << std::endl;

	// update the pollfd struct
	struct pollfd newClient = {newSocket, POLLIN, 0};
	mPollFds.push_back(newClient);

	// set the socket to be non-blocking
	setNonBlockingFlag(newSocket);

	// update clientState with new client
	Connection newCon = Connection(newSocket, clientIp.str());
	mClients[newSocket] = newCon;
}

void Reactor::removeConnection(int clientFd)
{
	LOG_NOTICE(std::string("closing connection ") + numToString(mPollFds[clientFd].fd));
	close(mPollFds[clientFd].fd);
	mClients.erase(mPollFds[clientFd].fd);
	mPollFds.erase(mPollFds.begin() + clientFd);
}

void Reactor::loop()
{
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
			if (i < mListenCount && (mPollFds[i].revents & POLLIN))
			{
				addConnection(mPollFds[i].fd);
			}
			else if (mPollFds[i].revents & POLLIN)
			{
				mClients[mPollFds[i].fd].onReadable();
				if (mClients[mPollFds[i].fd].parseRequest() == true)
					mPollFds[i].events |= POLLOUT;
			}
			else if (mPollFds[i].revents & POLLOUT)
			{
				if (mClients[mPollFds[i].fd].generateResponse() == true)
				{
					// if we have enough data, send reponse to client
					if (mClients[mPollFds[i].fd].onWritable() == true)
					{
						mClients[mPollFds[i].fd].responseReady = false;
						mClients[mPollFds[i].fd].parseRequest();
					}
					// if client requests to keep connection alive
					// if (mClients[mPollFds[i].fd].keepAlive == false)
					// 	mClients[mPollFds[i].fd].requestClose();
					continue;
				}
				// if all data has been sent, remove POLLOUT flag
				mPollFds[i].events &= ~POLLOUT;
			}
			if (mClients[mPollFds[i].fd].getKeepAlive() == false)
				removeConnection(mPollFds[i].fd);
		}
	}
}

void Reactor::handleEvent(int indx)
{
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
