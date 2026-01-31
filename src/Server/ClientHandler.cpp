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
	: mSocketFd(-1), mRequest(), mResponse(), mClientIp(), mBytesSent(0), mBytesRead(0), mParser(),
	  mResponseObj(), mCgiObj(), mResponseReady(false), mIsCgi(false), mIsCgiDone(false),
	  mPipeFd(0), mRequestNum(0)
{
	mParser.mResponse = &mResponseObj;
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
			 << int((clientAddr.sin_addr.s_addr & 0xFF000000) >> 24);

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
			LOG_NOTICE(std::string("client " + mClientIp + ", server " + mConfig.serverName +
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
				mParser.reset();
				mResponseObj.reset();
				parseRequest();
			}
			return;
		}
		pollStruct.events &= ~POLLOUT;
		mRequestNum++;
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

bool ClientHandler::writePost(std::string &uri, LocationConfig loc)
{
	std::string file = loc.root + uri;
	std::string temp = mParser.getPostFile();

	std::ifstream inf(temp.c_str());

	std::ofstream out(file.c_str(), std::ios::app);
	if (!inf || !out)
	{
		mResponseObj.setStatus(500);
		mResponseObj.setErrorMessage("Internal Server Error");
		return (false);
	}
	out << inf.rdbuf();
	out.close();
	std::remove(temp.c_str());
	return (true);
}

bool ClientHandler::deleteMethod(std::string &uri, LocationConfig loc)
{
	std::string file = loc.root + uri;

	if (access(file.c_str(), W_OK))
	{
		// TODO: insert no permission error msg nhere
		return (false);
	}
	if (remove(file.c_str()))
	{
		mResponseObj.setStatus(500);
		mResponseObj.setErrorMessage("Internal Server Error");
		return (false);
	}
	return (true);
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
		{
			if (mParser.getMethod() == POST)
			{
				for (int k = 0; k < locs[j].methods.size(); k++)
				{
					if (locs[j].methods[k] == "POST" && writePost(uri, locs[j]))
						return (true);
				}
			}
			else if (mParser.getMethod() == DELETE)
			{
				for (int l = 0; l < locs[j].methods.size(); l++)
				{
					if (locs[j].methods[l] == "DELETE" && deleteMethod(uri, locs[j]))
						return (true);
				}
			}
			else
				return (true);
		}
	}
	if (mResponseObj.mStatus < 400)
	{
		// TODO
		mResponseObj.setStatus(405);
	}
	return (false);
}

void ClientHandler::handleCookies()
{
	bool sessionFound = false;
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
	}
	if (!sessionFound)
	{
		// create new sesh ID
		std::string newId = generateId();
		session = mDispatch->addSession(newId);
	}
}

bool ClientHandler::generateResponse()
{
	ssize_t bytes = 0;

	mIsCgi = isCgi(this->mParser.getUri());
	if (mResponseReady == true)
		return (true);
	if (mParser.getParsingFinished() == false)
		return (false);
	handleCookies();
	// TODO:insert error checking ehre, don't post or delete or cgi if an error has been foudn
	mResponseReady = false;
	if (mIsCgi)
	{
		mDispatch->createCgiHandler(this);
	}
	else if (checkUri(mParser.getUri()) == false)
	{
		mResponseObj.setStatus(404);
		mResponseObj.setErrorMessage("Page not found!");
		if (mConfig.errorPages.find(404) != mConfig.errorPages.end())
		{
			mResponse = mResponseObj.buildResponse(mConfig.root.append(mConfig.errorPages[404]));
		}
		else
			mResponse = mResponseObj.buildErrorResponse();
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
