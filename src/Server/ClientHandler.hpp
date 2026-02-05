#pragma once

#include <string>
#include <sys/poll.h>
#include <unistd.h>

#include "CgiHandler.hpp"
#include "IHandler.hpp"
#include "RequestParser.hpp"
#include "ResponseBuilder.hpp"
#include "configParser.hpp"

class Dispatcher;

/**
	\class ClientHandler

	Event handler for a single client connection, acts as a bridge between
	RequestParser/ResponseBuilder and the client.
*/
class ClientHandler: public IHandler
{
public:
	ClientHandler();
	~ClientHandler();

	int				mSocketFd;
	std::string		mRequest;
	std::string		mClientIp;
	ssize_t			mBytesSent;
	ssize_t			mBytesRead;
	RequestParser	mParser;
	ResponseBuilder mResponseObj;
	CgiHandler	   *mCgiObj;
	bool			mIsCgi;
	bool			mIsCgiDone;
	Dispatcher	   *mDispatch;
	int				mPipeFd;
	int				mRequestNum;
	ServerConfig	*mConfig;

	bool acceptSocket(int listenFd, ServerConfig *srv, Dispatcher *dispatch);
	void setCgiFd(int pipeFd);
	void setCgiReady(bool status);
	void handleEvents(struct pollfd &pollStruct);
	bool writePost(std::string &uri, LocationConfig loc);
	bool deleteMethod(std::string &uri, LocationConfig loc);
	void handleCookies();

	bool		 getKeepAlive() const;
	int			 getFd() const;
	std::string &getResponse();

private:
	void readSocket();
	bool parseRequest();
	bool sendResponse();
	bool generateResponse();
	bool checkUri(std::string &uri);
};
