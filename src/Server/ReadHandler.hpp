#pragma once

#include <string>
#include <sys/poll.h>
#include <unistd.h>

#include "Acceptor.hpp"
#include "CgiHandler.hpp"
#include "IHandler.hpp"
#include "RequestParser.hpp"
#include "ResponseBuilder.hpp"
#include "configParser.hpp"

class Dispatcher;
struct Session;

/**
	\class ReadHandler

	Event handler for a single client connection, acts as a bridge between
	RequestParser/ResponseBuilder and the client.
*/
class ReadHandler: public IHandler
{
public:
	ReadHandler(int socketFd, ServerConfig *srv, Acceptor *acceptor, Dispatcher *dispatch);
	~ReadHandler();

	int				mSocketFd;
	int				mRequestNum;
	int				mPipeFd;
	bool			mIsCgi;
	bool			mIsCgiDone;
	ssize_t			mBytesSent;
	ssize_t			mBytesRead;
	std::string		mRequest;
	std::string		mClientIp;
	ServerConfig   *mConfig;
	Dispatcher	   *mDispatch;
	Acceptor	   *mAcceptor;
	RequestParser	mParser;
	ResponseBuilder mResponseObj;
	CgiHandler	   *mCgiObj;
	Session		   *mSession;

	void setCgiFd(int pipeFd);
	void setCgiReady(bool status);
	void handleEvents(struct pollfd &pollStruct);

	bool		 getKeepAlive() const;
	int			 getFd() const;

private:
	void readSocket();
	bool parseRequest();
};
