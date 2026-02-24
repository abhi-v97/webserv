#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

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
		}
	}
	else if (pollStruct.revents & POLLOUT)
	{
		if (generateResponse() == true)
		{
			if (sendResponse() == true)
			{
				mKeepAlive = mParser.getKeepAliveRequest();
				mRequestNum++;
				mBytesSent = 0;
				mIsCgi = false;
				mIsCgiDone = false;
				mParser.reset();
				mResponseObj.reset();
				parseRequest();
			}
			return;
		}
		// remove POLLOUT if you failed to generate a valid response
		pollStruct.events &= ~POLLOUT;
	}
}

// TODO: add logic for string.reserve to prevent repeat allocs
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

bool ClientHandler::parseRequest()
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

bool ClientHandler::generateResponse()
{
	if (mIsCgi == true && mIsCgiDone == false)
		return (true);
	if (mResponseObj.mResponseReady == true)
		return (true);
	if (mParser.getParsingFinished() == false)
		return (false);

	RouteResult route = mDispatch->getRouter().route(mParser, mConfig, mSession);
	setSession(route);
	if (route.type == RR_ERROR)
	{
		mResponseObj.buildErrorResponse(route);
	}
	else if (route.type == RR_BASIC)
	{
		mResponseObj.buildSimpleResponse(route.status, route.bodyMsg);
	}
	else if (route.type == RR_GET)
	{
		mResponseObj.buildResponse(route.filePath);
	}
	else if (route.type == RR_PARTIAL)
		mResponseObj.buildPartialResponse(route);
	else if (route.type == RR_CGI)
	{
		mIsCgi = true;
		mResponseObj.mResponseReady = true;
		mDispatch->createCgiHandler(this);
	}
	else
	{
		LOG_ERROR("RequestManager failed to find a valid route for this request");
	}
	return (true);
}

bool ClientHandler::sendResponse()
{
	ssize_t bytes = 0;

	if (mIsCgi == true && mIsCgiDone == false)
		return (false);
	if (mIsCgi == true)
	{
		bytes = send(mSocketFd,
					 mResponseObj.mResponse.c_str() + mBytesSent,
					 mResponseObj.mResponse.size() - mBytesSent,
					 MSG_NOSIGNAL);
	}
	else
	{
		bytes = send(mSocketFd,
					 mResponseObj.mResponse.c_str() + mBytesSent,
					 mResponseObj.mResponse.size() - mBytesSent,
					 MSG_NOSIGNAL);
	}
	if (bytes > 0)
		mBytesSent += bytes;
	else
		return (true);
	if (mResponseObj.mStatus >= 400)
	{
		LOG_ERROR("sendResponse: client " + mListener->getIp() + ", status " +
				  numToString(mResponseObj.mStatus) + " total size " + numToString(mBytesSent));
	}
	else
	{
		LOG_NOTICE("sendResponse: client " + mListener->getIp() + ", status " +
				   numToString(mResponseObj.mStatus) + ", total size " + numToString(mBytesSent));
	}
	return mBytesSent == mResponseObj.mResponse.size();
}

void ClientHandler::setSession(RouteResult &out)
{
	bool		 sessionFound = false;
	std::string &cookies = mParser.getCookies();
	Session		*session = NULL;
	std::string	 id;
	time_t		 now = time(NULL);

	size_t sessIdStart = cookies.find("session");
	if (sessIdStart != std::string::npos)
	{
		id = cookies.substr(sessIdStart + 8, 12);
		session = mDispatch->getSession(id);
		if (!session)
		{
			// TODO: check for invalid session ID
		}
		else if (mSession != session)
		{
			// TODO: wrong session ID provided, send an error and close connection
			LOG_ERROR("Session id mismatch");
			return;
		}
		else if (now - session->lastAccessed > 1800)
		{
			// TODO: maybe respond with a session timed out message?
			mDispatch->deleteSession(id);
		}
		else
		{
			session->lastAccessed = now;
			sessionFound = true;
		}
	}
	sessIdStart = cookies.find("session", sessIdStart + 8);
	if (sessIdStart != std::string::npos)
	{
		// TODO:: two sessions found...
		return;
	}
	if (!sessionFound)
	{
		// create new sesh ID
		id = generateId();
		mSession = mDispatch->addSession(id);
		out.sessionId = id;
		mResponseObj.setSessionId(id);
		cookies.append("session=" + id);
	}
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

std::string &ClientHandler::getResponse()
{
	return (this->mResponseObj.mResponse);
}
