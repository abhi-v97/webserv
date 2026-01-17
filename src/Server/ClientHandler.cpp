#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>

#include "CgiHandler.hpp"
#include "ClientHandler.hpp"
#include "Dispatcher.hpp"
#include "Logger.hpp"
#include "ResponseBuilder.hpp"
#include "Utils.hpp"
#include "configParser.hpp"

ClientHandler::ClientHandler()
	: mSocketFd(-1), mRequest(), mResponse(), mClientIp(), mBytesSent(0), mBytesRead(0),
	  mParser(RequestParser()), mResponseObj(), mCgiObj(), mResponseReady(false), mIsCgi(false),
	  mIsCgiDone(false)
{
}

bool ClientHandler::acceptSocket(int listenFd, ServerConfig srv, Dispatcher *dispatch)
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
	LOG_INFO(std::string("accept(): new client fd: ") + numToString(mSocketFd));

	// get client IP from sockaddr struct, store it as std::string
	ipStream << int(clientAddr.sin_addr.s_addr & 0xFF) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF00) >> 8) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF0000) >> 16) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF000000) >> 24) << std::endl;

	mClientIp = ipStream.str();
	mConfig = srv;
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
		{
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
				mParser.reset();
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

bool isCgi(std::string &uri)
{
	if (uri.find(".py") != std::string::npos || uri.find(".sh") != std::string::npos)
		return (true);
	return (false);
}

bool ClientHandler::checkUri(std::string &uri)
{
	std::vector<LocationConfig> &locs = this->mConfig.locations;
	
	int i = 1;
	for (; i < uri.size(); i++)
	{
		if (uri.at(i) == '/')
			break;
		i++;
	}
	std::string folder = uri.substr(0, i);
	for (int j = 0; j < locs.size(); j++)
	{
		if (folder == locs[j].path)
			return (true);
	}
	return (false);
}

bool ClientHandler::generateResponse()
{
	ssize_t bytes = 0;

	mIsCgi = isCgi(this->mParser.getUri());
	if (mResponseReady == true)
		return (true);
	if (mParser.getParsingFinished() == false)
		return (false);
	mResponseReady = false;
	if (mIsCgi)
	{
		mDispatch->createCgiHandler(this);
	}
	else if (checkUri(mParser.getUri()) == false)
	{
		mResponseObj.setStatus(404);
		if (mConfig.errorPages.find(404) != mConfig.errorPages.end())
		{
			std::string errorPage = mConfig.root;
			mResponse = mResponseObj.buildResponse(errorPage.append(mConfig.errorPages[404]));
		}
		else
		 	mResponse = mResponseObj.build404();
	}
	else
	{
		mResponseObj.parseRangeHeader(mParser);
		mResponse = mResponseObj.buildResponse(mParser.getUri());
	}
	mKeepAlive = mParser.getKeepAliveRequest();
	mResponseReady = true;
	mBytesSent = 0;
	return (true);
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
