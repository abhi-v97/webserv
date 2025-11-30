#pragma once

#include <string>
#include <sys/poll.h>
#include <unistd.h>

#include "CgiHandler.hpp"
#include "IHandler.hpp"
#include "RequestParser.hpp"
#include "ResponseBuilder.hpp"

class ClientHandler: public IHandler
{
public:
	ClientHandler();
	ClientHandler(int socket, const std::string &ipAddr);
	ClientHandler(const ClientHandler &obj);
	~ClientHandler();

	int				mFd;
	std::string		mRequest;
	std::string		response;
	std::string		clientIp;
	ssize_t			bytesSent;
	ssize_t			bytesRead;
	RequestParser	parser;
	ResponseBuilder responseObj;
	CgiHandler		cgiObj;
	bool			keepAlive;
	bool			responseReady;

	short events;

	void onReadable();	 // read + append to request
	bool parseRequest(); // parse from request, may produce response
	bool onWritable();	 // send remaining response
	bool generateResponse();
	bool getKeepAlive() const;

	void handleEvents(short revents);
	int	 getFd() const;
};
