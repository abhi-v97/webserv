#include <algorithm>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "CgiHandler.hpp"
#include "ClientHandler.hpp"
#include "Dispatcher.hpp"
#include "Logger.hpp"
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
			// generateResponse();
		}
	}
	else if (pollStruct.revents & POLLOUT)
	{
		if (generateResponse() == true)
		{
			if (sendResponse() == true)
			{
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

bool isCgi(std::string &uri)
{
	if (uri.find(".py") != std::string::npos || uri.find(".sh") != std::string::npos)
		return (true);
	return (false);
}

bool ClientHandler::writePost(const std::string &uri, LocationConfig loc)
{
	std::string file = loc.root + uri;
	std::string temp = mParser.getPostFile();

	std::ifstream inf(temp.c_str());

	std::ofstream out(file.c_str(), std::ios::app);
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
	std::string file = loc.root + uri;

	if (access(file.c_str(), W_OK))
	{
		// TODO: insert no permission error msg nhere
		return (false);
	}
	if (remove(file.c_str()))
	{
		// mResponseObj.setError(500, "Internal Server Error: Failed to remove the file");
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
			return (executeMethod(uri, mParser.getMethod(), locs[j]));
		}
	}
	return (false);
}

bool ClientHandler::executeMethod(const std::string &uri, RequestMethod method, LocationConfig &loc)
{
	std::string methodStr;

	if (method == POST)
		methodStr = "POST";
	else if (method == DELETE)
		methodStr = "DELETE";
	for (int i = 0; i < loc.methods.size(); i++)
	{
		if (loc.methods[i] == methodStr)
			break;
	}
	if (find(loc.methods.begin(), loc.methods.end(), methodStr) == loc.methods.end())
	{
		//  method not found
		return (false);
	}
	if ((method == POST && writePost(uri, loc)) || (method == DELETE && deleteMethod(uri, loc)))
		return (true);
	return (false);
}

void ClientHandler::handleCookies()
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
	// TODO:insert error checking ehre, don't post or delete or cgi if an error has been foudn
	handleCookies();
	if (mIsCgi)
	{
		mDispatch->createCgiHandler(this);
	}
	else if (validateUri(mParser.getUri()) == false)
	{
		// mResponseObj.setError(404, "Page not found!");
		if (mConfig->errorPages.find(404) != mConfig->errorPages.end())
		{
			mResponseObj.buildResponse(mConfig->root.append(mConfig->errorPages[404]));
		}
		else
		{
			mResponseObj.buildErrorResponse(404, "The requested file could not be found.");
		}
	}
	else
	{
		mResponseObj.parseRangeHeader(mParser);
		mResponseObj.buildResponse(mParser.getUri());
	}
	mKeepAlive = mParser.getKeepAliveRequest();
	mResponseObj.mResponseReady = true;
	return (true);
}

bool ClientHandler::sendResponse()
{
	ssize_t bytes = 0;

	if (mIsCgi == true && mIsCgiDone == false)
		return (false);
	bytes = send(mSocketFd,
				 mResponseObj.mResponse.c_str() + mBytesSent,
				 mResponseObj.mResponse.size() - mBytesSent,
				 MSG_NOSIGNAL);
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
