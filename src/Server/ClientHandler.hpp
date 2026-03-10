#pragma once

#include <string>
#include <sys/poll.h>
#include <unistd.h>

#include "CgiHandler.hpp"
#include "IHandler.hpp"
#include "Listener.hpp"
#include "RequestParser.hpp"
#include "ResponseBuilder.hpp"
#include "configParser.hpp"

class Dispatcher;
struct Session;

/**
	\class ClientHandler

	Event handler for a single client connection, acts as a bridge between
	RequestParser/ResponseBuilder and the client.
*/
class ClientHandler: public IHandler
{
public:
	ClientHandler(int socketFd, ServerConfig *srv, Listener *listener, Dispatcher *dispatch);

	int				mSocketFd;
	int				mRequestNum;
	int				mCgiFd;
	bool			mIsCgi;
	bool			mIsCgiDone;
	ssize_t			mBytesSent;
	time_t			mCgiStart;
	std::string		mRequest;
	std::string		mClientIp;
	ServerConfig   *mConfig;
	Dispatcher	   *mDispatch;
	Listener	   *mListener;
	RequestParser	mParser;
	ResponseBuilder mResponseObj;
	Session		   *mSession;

	void		 setCgiFd(int cgiFd);
	void		 setCgiReady(bool status);
	void		 handleEvents(struct pollfd &pollStruct);
	std::string &getResponse();

	bool			   getKeepAlive() const;
	int				   getFd() const;
	const std::string &getRequestBodyFile() const;

private:
	void readSocket();
	bool parseRequest();
	void setSession(RouteResult &out);

	bool generateResponse();
	bool sendResponse();
};
