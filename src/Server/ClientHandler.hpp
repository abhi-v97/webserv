#pragma once

#include <string>
#include <sys/poll.h>
#include <unistd.h>

#include "CgiHandler.hpp"
#include "IHandler.hpp"
#include "RequestParser.hpp"
#include "ResponseBuilder.hpp"

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
	std::string		mResponse;
	std::string		mClientIp;
	ssize_t			mBytesSent;
	ssize_t			mBytesRead;
	RequestParser	mParser;
	ResponseBuilder mResponseObj;
	CgiHandler	   *mCgiObj;
	bool			mKeepAlive;
	bool			mResponseReady;
	bool			mIsCgi;
	bool			mIsCgiDone;
	Dispatcher	   *mDispatch;
	int				mPipeFd;

	bool acceptSocket(int listenFd, Dispatcher *dispatch);
	void setCgiFd(int pipeFd);
	void setCgiReady(bool status);
	void handleEvents(struct pollfd &pollStruct);

	bool		 getKeepAlive() const;
	int			 getFd() const;
	std::string &getResponse();

private:
	void readSocket();
	bool parseRequest();
	bool sendResponse();
	bool generateResponse();
	void requestClose();
};
