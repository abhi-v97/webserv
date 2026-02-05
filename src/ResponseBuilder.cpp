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

ResponseBuilder::ResponseBuilder()
	: mResponseReady(false), mResponseStream(), mMin(0), mMax(0), mStatus(200)
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
	return mResponseStream.str();
}

void ResponseBuilder::reset()
{
	mStatus = 200;
	mMin = 0;
	mMax = 0;
	mResponseReady = false;
	mResponse.clear();
	mResponseStream.clear();
	mResponseStream.str("");
}

void ResponseBuilder::addCookies()
{
	std::string &cookies = mParser->getCookies();
	size_t		 cookieStart = 0;
	size_t		 cookieEnd = 0;

	while (42 && cookies.empty() == false)
	{
		cookieStart = cookies.find_first_not_of("; ", cookieStart);
		cookieEnd = cookies.find_first_of(';', cookieStart);
		if (cookieStart == std::string::npos)
			break;
		std::string temp = cookies.substr(cookieStart, cookieEnd - cookieStart);
		std::string cookieName = temp.substr(0, temp.find_first_of('='));
		if (cookieName != "last-accessed" && cookieName != "total-requests")
			mResponseStream << "set-cookie: " << temp << "\r\n";
		if (temp.empty())
			break;
		temp.clear();
		cookieStart = cookieEnd;
	}
	mResponseStream << "set-cookie: last-accessed=" << getTimestamp() << "\r\n";
	mResponseStream << "set-cookie: total-requests=" << mParser->getRequestCount() << "\r\n";
}

// simple response to a GET request
// grabs the file pointed to by uri and passes it along in its entirety
// now supports the content-type attribute, provided a matching extension exists in mime.types file
// if not, the file is treated as a plain text file
//
// Accept-ranges: bytes tells browser etc that our web server supports range
// requests, i.e browser can pause and resume downloads, or jump around in a
// video player
void ResponseBuilder::buildResponse(const std::string &uri)
{
	std::ostringstream body;
	std::string		   file = mConfig->root;
	file.append(uri);

	std::ifstream requestFile(file.c_str());

	// if the requested file is not found
	if (requestFile.good() == false)
	{
		buildErrorResponse(400, "The requested file could not be found");
		return;
	}
	body << requestFile.rdbuf();
	mResponseStream << "HTTP/1.1 " << mStatus << "\r\n";
	addCookies();
	mResponseStream << "Content-Type: " << MimeTypes::getInstance()->getType(uri)
			  << "\r\nAccept-Ranges: bytes\r\n";

	if (mMax == 0)
		mMax = body.str().size() - 1;

	if (mStatus == 206)
	{
		std::string newbody = body.str().substr(mMin, mMax - mMin + 1);
		mResponseStream << "Content-Range: bytes " << mMin << "-" << mMax << "/" << body.str().size();
		mResponseStream << "\r\nContent-Length: " << mMax - mMin + 1;
		mResponseStream << "\r\n\r\n" << newbody;
	}
	else
	{
		mResponseStream << "Content-Length: " << body.str().size();
		mResponseStream << "\r\n\r\n" << body.str();
	}
	mResponse = mResponseStream.str();
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
	mResponseStream << "HTTP/1.1 200\r\nContent-Type: text/html\r\nContent-Length: " << body.str().size()
			  << "\r\n\r\n"
			  << body.str();
	std::cout << mResponseStream.str() << std::endl;

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

// TODO: add a function that writes the name of the error code based on what error occurred next to
// the Error code, so for 404 it returns "Not Found", for 500 its "Internal Server Error"
// https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Status
void ResponseBuilder::buildErrorResponse(int code, const std::string &errorMsg)
{
	mResponseStream << "HTTP/1.1 " << numToString(code)
		 << "\r\nContent-Type: text/html\r\nContent-Length: " << numToString(80 + errorMsg.size())
		 << "\r\n\r\n<!DOCTYPE html><html lang=\"en\"><h1>Webserv</h1><h2>Error: "
		 << numToString(code) << "</h2><p> " << errorMsg << "</p></html>";
	mResponseReady = true;
	mStatus = code;
	mResponse = mResponseStream.str();
}

int ResponseBuilder::getStatus()
{
	return (mStatus);
}
