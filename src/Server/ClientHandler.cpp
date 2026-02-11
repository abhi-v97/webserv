#include <algorithm>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "CgiHandler.hpp"
#include "ClientHandler.hpp"
#include "Dispatcher.hpp"
#include "Logger.hpp"
#include "RequestParser.hpp"
#include "ResponseBuilder.hpp"
#include "Utils.hpp"
#include "configParser.hpp"

ClientHandler::ClientHandler()
	: mSocketFd(-1), mRequest(), mClientIp(), mBytesSent(0), mBytesRead(0), mParser(),
	  mResponseObj(), mCgiObj(), mIsCgi(false), mIsCgiDone(false), mPipeFd(0), mRequestNum(0)
{
	mParser.mResponse = &mResponseObj;
	mResponseObj.mParser = &mParser;
	mParser.mClient = this;
}

bool ClientHandler::acceptSocket(int listenFd, ServerConfig *srv, Dispatcher *dispatch)
{
	struct sockaddr_in clientAddr = {};
	socklen_t		   clientLen = sizeof(clientAddr);
	std::ostringstream ipStream;

	mSocketFd = accept(listenFd, (sockaddr *) &clientAddr, &clientLen);
	if (mSocketFd < 0)
		return (LOG_ERROR(std::string("acceptSocket(): ") + std::strerror(errno)), false);

	// get client IP from sockaddr struct, store it as std::string
	ipStream << int(clientAddr.sin_addr.s_addr & 0xFF) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF00) >> 8) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF0000) >> 16) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF000000) >> 24);

	mClientIp = ipStream.str();
	mConfig = srv;
	mResponseObj.mConfig = mConfig;
	mDispatch = dispatch;
	LOG_INFO(std::string("acceptSocket(): new client fd: ") + numToString(mSocketFd) +
			 "; ip: " + mClientIp);
	return (setNonBlockingFlag(mSocketFd));
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

// TODO: replace this with a config file setting, use shebang line to execute the file
bool isCgi(std::string &uri)
{
	if (uri.find(".py") != std::string::npos || uri.find(".sh") != std::string::npos)
		return (true);
	return (false);
}

bool ClientHandler::writePost(const std::string &uri, LocationConfig loc)
{
	std::string temp = mParser.getPostFile();

	std::ifstream inf(temp.c_str());
	std::ofstream out(uri.c_str(), std::ios::app);
	if (!inf || !out)
	{
		// mResponseObj.setError(500, "Internal Server Error: failed to write POST message");
		return (false);
	}
	out << inf.rdbuf();
	out.close();
	std::remove(temp.c_str());
	return (true);
}

bool ClientHandler::deleteMethod(const std::string &uri, LocationConfig loc)
{
	if (remove(uri.c_str()))
	{
		// mResponseObj.setError(500, "Internal Server Error: Failed to remove the file");
		return (false);
	}
	return (true);
}

/**
	\brief checks the full uri if the file has correct permissions

	\param uri must be the full path of the uri
	\param method request method
*/
bool ClientHandler::checkPermissions(const std::string &uri, RequestMethod method)
{
	if (access(uri.c_str(), F_OK))
	{
		if (method == GET || method == DELETE)
		{
			mResponseObj.buildErrorResponse(404, "Page not found");
			return (false);
		}
		else if (method == POST)
		{
			// POST method, okay if file doesn't exist
			mResponseObj.mStatus = 201;
			return (true);
		}
	}
	else if (method == GET && access(uri.c_str(), R_OK))
	{
		// TODO: test, see what nginx does whena requested file has no read permission
		mResponseObj.buildErrorResponse(403, "Failed to read resource");
		return (false);
	}
	else if (method == DELETE && access(uri.c_str(), W_OK))
	{
		mResponseObj.buildErrorResponse(403, "Failed to delete resource");
		return (false);
	}
	else if (mIsCgi == true && access(uri.c_str(), X_OK))
	{
		mResponseObj.buildErrorResponse(403, "Failed to execute resource");
		return (false);
	}
	return (true);
}

bool ClientHandler::validateUri(std::string &uri)
{
	std::vector<LocationConfig> &locs = this->mConfig->locations;

	size_t folderEnd = uri.rfind('/');
	if (folderEnd == std::string::npos || folderEnd == 0)
		folderEnd = 1;

	std::string folder = uri.substr(0, folderEnd);
	for (int j = 0; j < locs.size(); j++)
	{
		if (folder == locs[j].path)
		{
			uri = locs[j].root + uri;

			return (validateRequest(uri, locs[j], mParser.getMethod()));
		}
	}
	return (false);
}

bool ClientHandler::validateRequest(std::string			 &uri,
									const LocationConfig &loc,
									RequestMethod		  method)
{

	if (!checkPermissions(uri, method))
		return (false);
	if (!checkMethod(uri, method, loc))
	{
		mResponseObj.buildErrorResponse(403, "Method not allowed for this location");
		return (false);
	}
	if (method == GET)
		return (true);
	if ((method == POST && writePost(uri, loc)) || (method == DELETE && deleteMethod(uri, loc)))
		return (true);
	return (false);
}

bool ClientHandler::checkMethod(const std::string	 &uri,
								RequestMethod		  method,
								const LocationConfig &loc)
{
	std::string methodStr;

	if (method == POST)
		methodStr = "POST";
	else if (method == DELETE)
		methodStr = "DELETE";
	else if (method == GET)
		methodStr = "GET";
	for (int i = 0; i < loc.methods.size(); i++)
	{
		if (loc.methods[i] == methodStr)
			break;
	}
	if (find(loc.methods.begin(), loc.methods.end(), methodStr) == loc.methods.end())
		return (false);
	return (true);
}

void ClientHandler::setSession()
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
		else if (session != mSession)
		{
			// TODO: wrong session ID provided, send an error and close connection
			LOG_ERROR("Session id mismatch");
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
		session = mDispatch->addSession(id);
		mSession = session;
		mResponseObj.setSessionId(id);
		cookies.append("session=" + id);
	}
}

bool ClientHandler::generateResponse()
{
	ssize_t bytes = 0;

	mIsCgi = isCgi(this->mParser.getUri());
	if (mResponseObj.mResponseReady == true)
		return (true);
	if (mParser.getParsingFinished() == false)
		return (false);
	setSession();
	if (mIsCgi)
		mDispatch->createCgiHandler(this);
	else if (validateUri(mParser.getUri()))
	{
		mResponseObj.parseRangeHeader(mParser);
		mResponseObj.buildResponse(mParser.getUri());
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
		LOG_ERROR("sendResponse: client " + mClientIp + ", status " +
				  numToString(mResponseObj.mStatus) + " total size " + numToString(mBytesSent));
	}
	else
	{
		LOG_NOTICE("sendResponse: client " + mClientIp + ", status " +
				   numToString(mResponseObj.mStatus) + ", total size " + numToString(mBytesSent));
	}
	return mBytesSent == mResponseObj.mResponse.size();
}

std::string &ClientHandler::getResponse()
{
	return (this->mResponseObj.mResponse);
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
