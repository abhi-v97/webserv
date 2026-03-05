#include <cstddef>
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
	  mIsCgi(false), mIsCgiDone(false), mCgiFd(0), mRequestNum(0), mConfig(srv),
	  mDispatch(dispatch), mListener(listener)
{
	mSession = NULL;
	mParser.mResponse = &mResponseObj;
	mResponseObj.mParser = &mParser;
	mParser.mClient = this;
}

ClientHandler::~ClientHandler()
{
}

/**

*/
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
				mCgiStart = NULL;
				mParser.reset();
				mResponseObj.reset();
				// see if another request is queued up
				parseRequest();
			}
			return;
		}
		// remove POLLOUT if you failed to generate a valid response
		pollStruct.events &= ~POLLOUT;
	}
}

/**
	\brief Attempts to read from the socket looking for new HTTP requests
*/
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

	// reserve capacity
	if (mRequest.capacity() - mRequest.size() < static_cast<size_t>(bytesRead))
		mRequest.reserve(mRequest.size() + static_cast<size_t>(bytesRead) + 4096);
	mRequest.append(buffer, bytesRead);
}

/**
	\brief If enough request data has been received, begins the parsing process by handing off the
   data to RequestParser
*/
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

/**
	\brief Used as an intermediary function between request parsing and response generation.

	Reads the struct returned by RequestManager and routes the request to the correct destination.
*/
bool ClientHandler::generateResponse()
{
	if (mIsCgi == true && mIsCgiDone == false)
		return (true);
	if (mResponseObj.mResponseReady == true)
		return (true);
	if (mParser.getParsingFinished() == false)
		return (false);

	RouteResult route = mDispatch->getRouter().route(mParser, mConfig, mSession);
	route.keepAlive = mParser.getKeepAliveRequest();
	setSession(route);
	if (route.type == RR_ERROR)
		mResponseObj.buildErrorResponse(route);
	else if (route.type == RR_BASIC)
		mResponseObj.buildSimpleResponse(route.status, route.bodyMsg);
	else if (route.type == RR_GET)
		mResponseObj.buildResponse(route);
	else if (route.type == RR_PARTIAL)
		mResponseObj.buildPartialResponse(route);
	else if (route.type == RR_AUTOINDEX)
		mResponseObj.buildAutoIndex(route);
	else if (route.type == RR_REDIRECT)
		mResponseObj.buildRedirect(route);
	else if (route.type == RR_CGI || route.type == RR_CGI_POST)
	{
		mIsCgi = true;
		mResponseObj.mResponseReady = true;
		mDispatch->createCgiHandler(this, route);
		mCgiStart = time(NULL);
	}
	else
	{
		LOG_ERROR("RequestManager failed to find a valid route for this request");
	}
	return (true);
}

/**
	\brief Sends HTTP Response to the socket if its ready

	If a CGI process has stalled for over a minute, begins the shutdown process and sends an error
   message instead
*/
bool ClientHandler::sendResponse()
{
	ssize_t bytes = 0;

	if (mIsCgi == true && mIsCgiDone == false)
	{
		if (time(NULL) - mCgiStart > 60)
		{
			mResponseObj.buildSimpleResponse(500, "CGI Timeout Error");
			mDispatch->closeCgi(mCgiFd);
		}
		else
			return (false);
	}
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

/**
	\brief Session handler, validates the session and refreshes its duration or creates a new one
*/
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
		else if (!mSession)
		{
			mSession = session;
			session->lastAccessed = now;
			sessionFound = true;
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

/**
	\brief Returns a boolean instructing dispatcher whether to close the connection or not
*/
bool ClientHandler::getKeepAlive() const
{
	return (this->mKeepAlive);
}

/**
	\brief Returns the socket FD belonging to this particular connection
*/
int ClientHandler::getFd() const
{
	return (this->mSocketFd);
}

/**
	\brief Sets CGI status

	Used by CGI handler to signal the end of parsing/processing and to start sending the response
*/
void ClientHandler::setCgiReady(bool status)
{
	this->mIsCgiDone = status;
}

/**
	\brief stores the CGI fd in a variable

	Used by ClientHandler when CGI times out, the fd is needed to tell Dispatcher which cgi process
   to shut down
*/
void ClientHandler::setCgiFd(int cgiFd)
{
	this->mCgiFd = cgiFd;
}

/**
	\brief Getter for std::string that holds HTTP Response

	Used by CGI handler to set the response message
*/
std::string &ClientHandler::getResponse()
{
	return (this->mResponseObj.mResponse);
}

/**
	\brief Getter for temporary file which holds the request body

	Used as a getter by CgiHandler to access HTTP request body for POST requests
*/
const std::string &ClientHandler::getRequestBodyFile() const
{
	return (this->mParser.getBodyFile());
}
