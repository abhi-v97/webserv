#include <netinet/in.h>
#include <sys/socket.h>

#include "CgiHandler.hpp"
#include "ClientHandler.hpp"
#include "Logger.hpp"
#include "ResponseBuilder.hpp"
#include "Utils.hpp"

ClientHandler::ClientHandler()
	: mSocketFd(-1), mRequest(), response(), mClientIp(), bytesSent(0), bytesRead(0),
	  parser(RequestParser()), responseObj(ResponseBuilder()), cgiObj(CgiHandler()),
	  keepAlive(true), responseReady(false)
{
}

bool ClientHandler::acceptSocket(int listenFd)
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
	LOG_NOTICE(std::string("accepted client with fd: ") + numToString(mSocketFd));

	// get client IP from sockaddr struct, store it as std::string
	ipStream << int(clientAddr.sin_addr.s_addr & 0xFF) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF00) >> 8) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF0000) >> 16) << "."
			 << int((clientAddr.sin_addr.s_addr & 0xFF000000) >> 24) << std::endl;
	std::cout << "TEST: clientIp: " << ipStream.str() << std::endl;

	mClientIp = ipStream.str();
	return (setNonBlockingFlag(mSocketFd));
}

ClientHandler::ClientHandler(int socket, const std::string &ipAddr)
{
	bytesRead = 0;
	bytesSent = 0;
	mSocketFd = socket;
	mClientIp = ipAddr;
	parser = RequestParser();
	cgiObj = CgiHandler();
	responseReady = false;
	keepAlive = true;
}

ClientHandler::ClientHandler(const ClientHandler &obj)
{
	bytesRead = obj.bytesRead;
	bytesSent = obj.bytesSent;
	mSocketFd = obj.mSocketFd;
	mClientIp = obj.mClientIp;
	parser = obj.parser;
	cgiObj = obj.cgiObj;
	responseReady = obj.responseReady;
	keepAlive = obj.keepAlive;
}

ClientHandler::~ClientHandler()
{
}

void ClientHandler::handleEvents(short revents)
{
	if (revents & POLLIN)
		;
	else if (revents & POLLOUT)
		;
}

int ClientHandler::getFd() const
{
	return (this->mSocketFd);
}

void ClientHandler::onReadable()
{
	char	buffer[4096];
	ssize_t bytesRead;

	// don't read next data until you are finished with all requests from the previous read
	if (parser.getParsingFinished())
		return;

	bytesRead = recv(mSocketFd, buffer, 4096, 0);
	if (bytesRead < 0)
	{
		LOG_WARNING("recv() failed to read from socket");
		return;
	}
	else if (bytesRead == 0)
	{
		LOG_NOTICE(std::string("recv(): zero bytes received, closing connection ") +
				   numToString(mSocketFd));
		keepAlive = false;
		return;
	}
	mRequest.append(buffer, bytesRead);
}

bool ClientHandler::onWritable()
{
	ssize_t bytes = 0;

	while (bytesSent < response.size())
	{
		bytes = send(
			mSocketFd, response.c_str() + bytesSent, response.size() - bytesSent, MSG_NOSIGNAL);
		if (bytes < 0)
			return (false);
		bytesSent += bytes;
	}
	// return true if done, false if not yet finished
	return bytesSent == response.size();
	return (true);
}

bool ClientHandler::generateResponse()
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

bool ClientHandler::parseRequest()
{
	if (mRequest.empty())
		return (false);

	if (parser.parse(mRequest) == false)
	{
		LOG_ERROR(std::string("invalid request, closing connection ") + numToString(mSocketFd));
		keepAlive = false;
		return (false);
	}
	return (parser.getParsingFinished());
}

bool ClientHandler::getKeepAlive() const
{
	return (this->keepAlive);
}
