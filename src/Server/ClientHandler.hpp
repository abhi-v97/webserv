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
	CgiHandler		mCgiObj;
	bool			mKeepAlive;
	bool			mResponseReady;

	Dispatcher *mDispatch;

	bool acceptSocket(int listenFd, Dispatcher *dispatch);
	bool getKeepAlive() const;

	void handleEvents(struct pollfd &pollStruct);
	int	 getFd() const;

private:
	void readSocket();
	bool parseRequest();
	bool sendResponse();
	bool generateResponse();
	void requestClose();
};
