#include <sys/socket.h>

#include "Connection.hpp"
#include "Logger.hpp"

Connection::Connection()
{
}

Connection::Connection(int socket, const std::string &ipAddr)
{
	bytesRead = 0;
	bytesSent = 0;
	mFd = socket;
	clientIp = ipAddr;
	parser = RequestParser();
	cgiObj = CgiHandler();
	responseReady = false;
	keepAlive = true;
}

Connection::Connection(const Connection &obj)
{
	bytesRead = obj.bytesRead;
	bytesSent = obj.bytesSent;
	mFd = obj.mFd;
	clientIp = obj.clientIp;
	parser = obj.parser;
	cgiObj = obj.cgiObj;
	responseReady = obj.responseReady;
	keepAlive = obj.keepAlive;
}

Connection::~Connection()
{
}

void Connection::onReadable()
{
	char	buffer[4096];
	ssize_t bytesRead;

	// don't read next data until you are finished with all requests from the previous read
	if (parser.getParsingFinished())
		return;

	bytesRead = recv(mFd, buffer, 4096, 0);
	if (bytesRead < 0)
	{
		LOG_WARNING("recv() failed to read from socket");
		return;
	}
	else if (bytesRead == 0)
	{
		LOG_NOTICE(std::string("recv(): zero bytes received, closing connection ") +
				   numToString(mFd));
		keepAlive = false;
		return;
	}
	mRequest.append(buffer, bytesRead);
}

bool Connection::onWritable()
{
	ssize_t bytes = 0;

	while (bytesSent < response.size())
	{
		bytes = send(mFd, response.c_str() + bytesSent, response.size() - bytesSent, MSG_NOSIGNAL);
		if (bytes < 0)
			return (false);
		bytesSent += bytes;
	}
	// return true if done, false if not yet finished
	return bytesSent == response.size();
	return (true);
}

bool Connection::generateResponse()
{
	ssize_t bytes = 0;
	int		isCGI = 0;

	if (responseReady == true)
		return (true);
	if (parser.getParsingFinished() == false)
		return (false);
	responseReady = false;
	if (isCGI)
	{
		// TODO: change this to accept other CGI requests
		cgiObj.execute("hello.py");

		int outfd = cgiObj.getOutFd();
		if (responseObj.readCgiResponse(outfd))
		{
			events |= POLLOUT;
		}
		response = responseObj.getResponse();
	}
	else
	{
		response = responseObj.buildResponse();
	}
	keepAlive = parser.keepAlive();
	parser.reset();
	responseReady = true;
	bytesSent = 0;
	return (true);
	return (true);
}

bool Connection::parseRequest()
{
	if (mRequest.empty())
		return (false);

	if (parser.parse(mRequest) == false)
	{
		LOG_ERROR(std::string("invalid request, closing connection ") + numToString(mFd));
		keepAlive = false;
		return (false);
	}
	return (parser.getParsingFinished());
}

bool Connection::getKeepAlive() const
{
	return (this->keepAlive);
}
