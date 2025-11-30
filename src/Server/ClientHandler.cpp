#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>

#include "CgiHandler.hpp"
#include "ClientHandler.hpp"
#include "Dispatcher.hpp"
#include "Logger.hpp"
#include "ResponseBuilder.hpp"
#include "Utils.hpp"

ClientHandler::ClientHandler()
	: mSocketFd(-1), mRequest(), mResponse(), mClientIp(), mBytesSent(0), mBytesRead(0),
	  mParser(RequestParser()), mResponseObj(ResponseBuilder()), mCgiObj(CgiHandler()),
	  mKeepAlive(true), mResponseReady(false)
{
}

bool ClientHandler::acceptSocket(int listenFd, Dispatcher *dispatch)
{
	struct sockaddr_in clientAddr = {};
	socklen_t		   clientLen = sizeof(clientAddr);
	std::ostringstream ipStream;

	mSocketFd = accept(listenFd, (sockaddr *) &clientAddr, &clientLen);
	if (mSocketFd < 0)
	{
		LOG_ERROR(std::string("accept() failed: ") + std::strerror(errno));
		return (false);
	}
	LOG_NOTICE(std::string("accepted client with fd: ") + numToString(mSocketFd));

	// get client IP from sockaddr struct, store it as std::string
	ipStream << int(clientAddr.sin_addr.s_addr & 0xFF) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF00) >> 8) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF0000) >> 16) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF000000) >> 24) << std::endl;
	std::cout << "TEST: clientIp: " << ipStream.str() << std::endl;

	mClientIp = ipStream.str();
	mDispatch = dispatch;
	return (setNonBlockingFlag(mSocketFd));
}

ClientHandler::~ClientHandler()
{
}

void ClientHandler::handleEvents(pollfd &pollStruct)
{
	if (pollStruct.revents & POLLIN)
	{
		this->readSocket();
		if (this->parseRequest() == true)
			pollStruct.events |= POLLOUT;
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
		LOG_WARNING("recv() failed to read from socket");
		return;
	}
	else if (bytesRead == 0)
	{
		LOG_NOTICE(std::string("recv(): zero bytes received, closing connection ") +
				   numToString(mSocketFd));
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
		LOG_ERROR(std::string("invalid request, closing connection ") + numToString(mSocketFd));
		mKeepAlive = false;
		return (false);
	}
	return (mParser.getParsingFinished());
}

bool ClientHandler::sendResponse()
{
	ssize_t bytes = 0;

	while (mBytesSent < mResponse.size())
	{
		bytes = send(
			mSocketFd, mResponse.c_str() + mBytesSent, mResponse.size() - mBytesSent, MSG_NOSIGNAL);
		if (bytes < 0)
			return (false);
		mBytesSent += bytes;
	}
	// return true if done, false if not yet finished
	return mBytesSent == mResponse.size();
}

bool ClientHandler::generateResponse()
{
	ssize_t bytes = 0;
	int		isCGI = 0;

	if (mResponseReady == true)
		return (true);
	if (mParser.getParsingFinished() == false)
		return (false);
	mResponseReady = false;
	if (isCGI)
	{
		// TODO: change this to accept other CGI requests
		mCgiObj.execute("hello.py");

		int outfd = mCgiObj.getOutFd();
		mResponseObj.readCgiResponse(outfd);
		mResponse = mResponseObj.getResponse();
	}
	else
	{
		mResponse = mResponseObj.buildResponse();
	}
	mKeepAlive = mParser.keepAlive();
	mParser.reset();
	mResponseReady = true;
	mBytesSent = 0;
	return (true);
	return (true);
}

bool ClientHandler::getKeepAlive() const
{
	return (this->mKeepAlive);
}

int ClientHandler::getFd() const
{
	return (this->mSocketFd);
}

void ClientHandler::requestClose()
{
	close(mSocketFd);
	mSocketFd = -1;
}
