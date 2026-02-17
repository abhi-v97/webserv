#pragma once

#include <string>
#include <unistd.h>

#include "Acceptor.hpp"
#include "Dispatcher.hpp"
#include "IHandler.hpp"
#include "RequestParser.hpp"
#include "configParser.hpp"

class WriteHandler: public IHandler
{
public:
	WriteHandler(int socketFd, RequestParser &requestObj, ServerConfig *srv);

	void setCgiFd(int pipeFd);
	void setCgiReady(bool status);
	void handleEvents(struct pollfd &pollStruct);
	int	 getFd() const;
	bool getKeepAlive() const;
	void setBodyFile(const std::string &bodyFile);
	std::string &getResponse();

private:
	bool			mSocketFd;
	bool			mKeepAlive;
	bool			mIsCgi;
	bool			mIsCgiDone;
	bool			mHasBody;
	int				mPipeFd;
	ssize_t			mBytesSent;
	Acceptor	   *mAcceptor;
	Dispatcher	   *mDispatch;
	RequestParser  &mParser;
	ResponseBuilder mResponseObj;
	ServerConfig   *mConfig;
	Session		   *mSession;

	bool generateResponse();
	bool sendResponse();
	bool writePost(const std::string &uri, LocationConfig loc);
	bool deleteMethod(const std::string &uri, LocationConfig loc);
	bool checkPermissions(const std::string &uri, RequestMethod method);
	bool validateUri(std::string &uri);
	bool validateRequest(std::string &uri, const LocationConfig &loc, RequestMethod method);
	bool checkMethod(const std::string &uri, RequestMethod method, const LocationConfig &loc);
	void setSession();
};
