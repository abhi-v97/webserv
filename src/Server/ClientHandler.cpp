#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>

#include "CgiHandler.hpp"
#include "ClientHandler.hpp"
#include "Dispatcher.hpp"
#include "Logger.hpp"
#include "RequestManager.hpp"
#include "RequestParser.hpp"
#include "ResponseBuilder.hpp"
#include "Utils.hpp"
#include "configParser.hpp"

ClientHandler::ClientHandler(int		   socketFd,
							 ServerConfig *srv,
							 Listener	  *listener,
							 Dispatcher	  *dispatch)
	: mSocketFd(socketFd), mRequest(), mClientIp(), mBytesSent(0), mParser(), mResponseObj(),
	  mCgiObj(), mIsCgi(false), mIsCgiDone(false), mPipeFd(0), mRequestNum(0), mConfig(srv),
	  mDispatch(dispatch), mListener(listener)
{
	mParser.mResponse = &mResponseObj;
	mResponseObj.mParser = &mParser;
	mParser.mClient = this;
}

ClientHandler::~ClientHandler()
{
}

void ClientHandler::handleEvents(pollfd &pollStruct)
{
	if (pollStruct.revents & POLLIN && mParser.getParsingFinished() == false)
	{
		this->readSocket();
		if (this->parseRequest() == true)
		{
			LOG_NOTICE(std::string("client " + mClientIp + ", server " + mConfig->serverName +
								   ", request: \"" + mParser.getRequestHeader() + "\""));
			pollStruct.events |= POLLOUT;
			generateResponse();
		}
	}
	else if (pollStruct.revents & POLLOUT)
	{
		if (this->generateResponse() == true)
		{
			if (this->sendResponse() == true)
			{
				mResponseReady = false;
				parseRequest();
			}
			return;
		}
		pollStruct.events &= ~POLLOUT;
	}
}

void ClientHandler::readSocket()
{
	char	buffer[4096];
	ssize_t bytesRead;

	// don't read next data until you are finished with all requests from the previous read
	if (mParser.getParsingFinished())
		return;

	bytesRead = recv(mSocketFd, buffer, 4096, 0);
	if (bytesRead < 0)
	{
		LOG_WARNING(std::string("recv(): client " + numToString(mSocketFd)) +
					": failed to read from socket");
		return;
	}
	else if (bytesRead == 0)
	{
		LOG_NOTICE(std::string("recv(): client " + numToString(mSocketFd)) +
				   ": zero bytes received, closing connection ");
		mKeepAlive = false;
		return;
	}
	mRequest.append(buffer, bytesRead);
}

bool ClientHandler::parseRequest()
{
	if (mRequest.empty())
		return (false);

	if (mParser.parse(mRequest) == false)
	{
		LOG_ERROR(std::string("parser(): client " + numToString(mSocketFd) +
							  ": invalid request, closing connection "));
		mKeepAlive = false;
		return (false);
	}
	return (mParser.getParsingFinished());
}

bool ClientHandler::sendResponse()
{
	ssize_t bytes = 0;

	if (mIsCgi == true && mIsCgiDone == false)
		return (false);
	bytes = send(
		mSocketFd, mResponse.c_str() + mBytesSent, mResponse.size() - mBytesSent, MSG_NOSIGNAL);
	if (bytes > 0)
		mBytesSent += bytes;
	else
		return (true);
	LOG_NOTICE("response(): client: " + numToString(mSocketFd) + ": status " +
			   numToString(mResponseObj.mStatus) + " total size " + numToString(mBytesSent));
	return mBytesSent == mResponse.size();
}

bool ClientHandler::generateResponse()
{
	ssize_t bytes = 0;

	mIsCgi = false;
	if (mResponseReady == true)
		return (true);
	if (mParser.getParsingFinished() == false)
		return (false);
	mResponseReady = false;
	if (mIsCgi)
	{
		mDispatch->createCgiHandler(this);
	}
	else
	{
		mResponseObj.parseRangeHeader(mParser);
		mResponse = mResponseObj.buildResponse(mParser.getUri());
	}
	mKeepAlive = mParser.getKeepAliveRequest();
	mParser.reset();
	mResponseReady = true;
	mBytesSent = 0;
	return (true);
}

std::string &ClientHandler::getResponse()
{
	return (this->mResponse);
}

bool ClientHandler::getKeepAlive() const
{
	return (this->mKeepAlive);
}

int ClientHandler::getFd() const
{
	return (this->mSocketFd);
}

void ClientHandler::setCgiReady(bool status)
{
	this->mIsCgiDone = status;
}

void ClientHandler::setCgiFd(int pipeFd)
{
	this->mPipeFd = pipeFd;
}
