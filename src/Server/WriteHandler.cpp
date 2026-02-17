#include "RequestParser.hpp"
#include "Utils.hpp"
#include "WriteHandler.hpp"
#include "configParser.hpp"
#include "Logger.hpp"
#include <algorithm>

WriteHandler::WriteHandler(int socketFd, RequestParser &requestObj, ServerConfig *srv) : mSocketFd(socketFd), mParser(requestObj), mConfig(srv)
{
	mKeepAlive = false;	
}

void WriteHandler::handleEvents(struct pollfd &pollStruct)
{
	if (pollStruct.revents & POLLOUT)
	{
		if (generateResponse() == true)
		{
			if (sendResponse() == true)
			{
				// mKeepAlive = mParser.getKeepAliveRequest();
				// mRequestNum++;
				mBytesSent = 0;
				// mParser.reset();
				mResponseObj.reset();
				// parseRequest();
			}
			return;
		}
		// remove POLLOUT if you failed to generate a valid response
		pollStruct.events &= ~POLLOUT;
	}
}


// TODO: replace this with a config file setting, use shebang line to execute the file
bool isCgi(std::string &uri)
{
	if (uri.find(".py") != std::string::npos || uri.find(".sh") != std::string::npos)
		return (true);
	return (false);
}

bool WriteHandler::writePost(const std::string &uri, LocationConfig loc)
{
	const std::string &bodyFile = mParser.getBodyFile();

	if (mHasBody == false)
		return (true);
	std::ifstream inf(bodyFile.c_str());
	std::ofstream out(uri.c_str(), std::ios::app);
	if (!inf || !out)
	{
		// mResponseObj.setError(500, "Internal Server Error: failed to write POST message");
		return (false);
	}
	out << inf.rdbuf();
	out.close();
	std::remove(bodyFile.c_str());
	return (true);
}

bool WriteHandler::deleteMethod(const std::string &uri, LocationConfig loc)
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
bool WriteHandler::checkPermissions(const std::string &uri, RequestMethod method)
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

bool WriteHandler::validateUri(std::string &uri)
{
	std::vector<LocationConfig> &locs = mConfig->locations;

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

bool WriteHandler::validateRequest(std::string			 &uri,
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

bool WriteHandler::checkMethod(const std::string	 &uri,
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

void WriteHandler::setSession()
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

bool WriteHandler::generateResponse()
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

bool WriteHandler::sendResponse()
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
		LOG_ERROR("sendResponse: client " + mAcceptor->getIp() + ", status " +
				  numToString(mResponseObj.mStatus) + " total size " + numToString(mBytesSent));
	}
	else
	{
		LOG_NOTICE("sendResponse: client " + mAcceptor->getIp() + ", status " +
				   numToString(mResponseObj.mStatus) + ", total size " + numToString(mBytesSent));
	}
	return mBytesSent == mResponseObj.mResponse.size();
}

int WriteHandler::getFd() const
{
	return (mSocketFd);
}

bool WriteHandler::getKeepAlive() const
{
	return (mKeepAlive);
}

void WriteHandler::setCgiReady(bool status)
{
	this->mIsCgiDone = status;
}

void WriteHandler::setCgiFd(int pipeFd)
{
	this->mPipeFd = pipeFd;
}

std::string &WriteHandler::getResponse()
{
	return (this->mResponseObj.mResponse);
}
