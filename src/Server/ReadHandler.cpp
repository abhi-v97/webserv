#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "Dispatcher.hpp"
#include "Logger.hpp"
#include "ReadHandler.hpp"
#include "RequestParser.hpp"
#include "ResponseBuilder.hpp"
#include "configParser.hpp"

ReadHandler::ReadHandler(int socketFd, ServerConfig *srv, Acceptor *acceptor, Dispatcher *dispatch)
	: mSocketFd(socketFd), mRequest(), mClientIp(), mBytesSent(0), mBytesRead(0), mParser(),
	  mResponseObj(), mCgiObj(), mIsCgi(false), mIsCgiDone(false), mPipeFd(0), mRequestNum(0),
	  mConfig(srv), mDispatch(dispatch), mAcceptor(acceptor)
{
	mParser.mResponse = &mResponseObj;
	mResponseObj.mParser = &mParser;
	mParser.mClient = this;
}

ReadHandler::~ReadHandler()
{
}

void ReadHandler::handleEvents(pollfd &pollStruct)
{
	if (pollStruct.revents & POLLIN && mParser.getParsingFinished() == false)
	{
		this->readSocket();
		if (this->parseRequest() == true)
		{
			LOG_NOTICE(std::string("client " + mClientIp + ", server " + mConfig->serverName +
								   ", request: \"" + mParser.getRequestHeader() + "\""));
			pollStruct.events |= POLLOUT;
		}
	}
}

// TODO: add logic for string.reserve to prevent repeat allocs
void ReadHandler::readSocket()
{
	char	buffer[4096];
	ssize_t bytesRead;

	// don't read next data until you are finished with all requests from the previous read
	if (mParser.getParsingFinished())
		return;

	bytesRead = recv(mSocketFd, buffer, 4096, 0);
	if (bytesRead < 0)
	{
		LOG_WARNING(std::string("readSocket(): client " + numToString(mSocketFd)) +
					": failed to read from socket");
		return;
	}
	else if (bytesRead == 0)
	{
		LOG_NOTICE(std::string("readSocket(): client " + numToString(mSocketFd)) +
				   ": zero bytes received, closing connection");
		mKeepAlive = false;
		return;
	}
	mRequest.append(buffer, bytesRead);
}

bool ReadHandler::parseRequest()
{
	if (mRequest.empty())
		return (false);

	if (mParser.parse(mRequest) == false)
	{
		// TODO: close the connection only for terrible requests
		// if the request error was 400 (bad format) or 404 (file not found), there's no need to
		// close the connection straight away
		// close it for errors where we do not know where the next request may start on the socket
		// stream, eg payload too large, 414 uri too long, or 408 request timeout (if implemented)

		// LOG_ERROR(std::string("parseRequest(): client " + numToString(mSocketFd) +
		// 					  ": invalid HTTP request, closing connection"));
		// mKeepAlive = false;
		// return (false);
		mRequest.clear();
	}
	return (mParser.getParsingFinished());
}

bool ReadHandler::getKeepAlive() const
{
	return (this->mKeepAlive);
}

int ReadHandler::getFd() const
{
	return (this->mSocketFd);
}

void ReadHandler::setCgiReady(bool status)
{
	this->mIsCgiDone = status;
}

void ReadHandler::setCgiFd(int pipeFd)
{
	this->mPipeFd = pipeFd;
}
