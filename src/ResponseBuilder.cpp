#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unistd.h>

#include "Logger.hpp"
#include "MimeTypes.hpp"
#include "RequestParser.hpp"
#include "ResponseBuilder.hpp"

#define BUFFER_SIZE 4096

/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

ResponseBuilder::ResponseBuilder(): mResponse(), mMin(0), mMax(0), mStatus(200)
{
}

/*
** -------------------------------- DESTRUCTOR --------------------------------
*/

ResponseBuilder::~ResponseBuilder()
{
}

/*
** --------------------------------- OVERLOAD ---------------------------------
*/

std::string ResponseBuilder::getResponse()
{
	return mResponse.str();
}

void ResponseBuilder::setStatus(int code)
{
	mStatus = code;
}

void ResponseBuilder::setErrorMessage(const std::string &error)
{
	this->mErrorMsg = error;
}

void ResponseBuilder::reset()
{
	mStatus = 200;
	mMin = 0;
	mMax = 0;
	mErrorMsg.clear();
	mResponse.clear();
}

void ResponseBuilder::addCookies()
{
	std::string &cookies = mParser->getCookies();
	size_t		 cookieStart = 0;
	size_t		 cookieEnd = 0;

	while (42)
	{
		cookieStart = cookies.find_first_not_of(' ', cookieStart);
		cookieEnd = cookies.find_first_of(';', cookieStart);
		std::string temp = cookies.substr(cookieStart, cookieEnd - cookieStart);
		if (temp.empty())
			break;
		mResponse << "set-cookie: " << temp << "\r\n";
		temp.clear();
	}
}

// simple response to a GET request
// grabs the file pointed to by uri and passes it along in its entirety
// now supports the content-type attribute, provided a matching extension exists in mime.types file
// if not, the file is treated as a plain text file
//
// Accept-ranges: bytes tells browser etc that our web server supports range
// requests, i.e browser can pause and resume downloads, or jump around in a
// video player
std::string ResponseBuilder::buildResponse(const std::string &uri)
{
	std::ifstream	   requestFile(uri.c_str());
	std::ostringstream body;

	body << requestFile.rdbuf();
	mResponse << "HTTP/1.1 " << mStatus << "\r\n";
	addCookies();
	mResponse << "Content-Type: " << MimeTypes::getInstance()->getType(uri)
			  << "\r\nAccept-Ranges: bytes\r\n";

	if (mMax == 0)
		mMax = body.str().size() - 1;

	if (mStatus == 206)
	{
		std::string newbody = body.str().substr(mMin, mMax - mMin + 1);
		mResponse << "Content-Range: bytes " << mMin << "-" << mMax << "/" << body.str().size();
		mResponse << "\r\nContent-Length: " << mMax - mMin + 1;
		mResponse << "\r\n\r\n" << newbody;
	}
	else
	{
		mResponse << "Content-Length: " << body.str().size();
		mResponse << "\r\n\r\n" << body.str();
	}
	return mResponse.str();
}

bool ResponseBuilder::readCgiResponse(int pipeOutFd)
{
	std::ostringstream body;

	char buffer[BUFFER_SIZE];
	memset(buffer, 0, BUFFER_SIZE);

	ssize_t len = read(pipeOutFd, buffer, BUFFER_SIZE);
	while (len > 0)
	{
		body << buffer;
		len = read(pipeOutFd, buffer, BUFFER_SIZE);
	}
	mResponse << "HTTP/1.1 200\r\nContent-Type: text/html\r\nContent-Length: " << body.str().size()
			  << "\r\n\r\n"
			  << body.str();
	std::cout << mResponse.str() << std::endl;

	// returns true if len isn't positive, meaning read is finished
	return len <= 0;
}

void ResponseBuilder::parseRangeHeader(RequestParser &parser)
{
	std::string rangeStr;

	rangeStr = parser.getHeaders()["range"];

	if (rangeStr.empty())
	{
		mStatus = 200;
		return;
	}
	else
	{
		mStatus = 206;
		size_t equal = rangeStr.find_first_of('=');
		size_t dash = rangeStr.find_first_of('-');

		mMin = std::atoi(rangeStr.c_str() + equal + 1);
		mMax = std::atoi(rangeStr.c_str() + dash + 1);
		std::cout << "minStr: " << mMin << ", maxStr: " << mMax << std::endl;
	}
}

std::string ResponseBuilder::buildErrorResponse()
{
	std::stringstream body;

	body << "HTTP/1.1 " << numToString(mStatus)
		 << "\r\nContent-Type: text/html\r\nContent-Length: " << numToString(80 + mErrorMsg.size())
		 << "\r\n\r\n<!DOCTYPE html><html lang=\"en\"><h1>Webserv</h1><h2>Error: "
		 << numToString(mStatus) << "</h2><p> " << mErrorMsg << "</p></html>";
	return (body.str());
}

int ResponseBuilder::getStatus()
{
	return (mStatus);
}
