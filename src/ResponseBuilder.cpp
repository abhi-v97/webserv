#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unistd.h>

#include "Logger.hpp"
#include "MimeTypes.hpp"
#include "RequestManager.hpp"
#include "RequestParser.hpp"
#include "ResponseBuilder.hpp"

#define BUFFER_SIZE 4096

/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

ResponseBuilder::ResponseBuilder()
	: mResponseReady(false), mNewSession(false), mResponseStream(), mMin(0), mMax(0), mStatus(200)
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
	mNewSession = false;
	mResponse.clear();
	mResponseStream.clear();
	mResponseStream.str("");
	mSessionId.clear();
}

void ResponseBuilder::addCookies()
{
	mResponseStream << "Set-Cookie: last-accessed=" << getTimestamp() << "; Path=/;\r\n";
	mResponseStream << "Set-Cookie: total-requests=" << mParser->getRequestCount()
					<< "; Path=/;\r\n";
	if (mNewSession == true)
		mResponseStream << "Set-Cookie: session=" << mSessionId << "; Path=/;\r\n";
}

// simple response to a GET request
// grabs the file pointed to by uri and passes it along in its entirety
// now supports the content-type attribute, provided a matching extension exists in mime.types file
// if not, the file is treated as a plain text file
//
// Accept-ranges: bytes tells browser etc that our web server supports range
// requests, i.e browser can pause and resume downloads, or jump around in a
// video player
void ResponseBuilder::buildResponse(RouteResult &route)
{
	std::ostringstream body;
	std::ifstream	   requestFile(route.filePath.c_str());

	// if the requested file could not be opened
	if (requestFile.good() == false)
	{
		buildSimpleResponse(500, "The requested file could not be accessed");
		return;
	}
	body << requestFile.rdbuf();
	mStatus = route.status;
	mResponseStream << "HTTP/1.1 " << mStatus << "\r\n";
	addCookies();
	mResponseStream << "Content-Type: " << MimeTypes::getInstance()->getType(route.filePath)
					<< "\r\nAccept-Ranges: bytes\r\n";
	addConnectionField(route.keepAlive);
	if (mParser->getMethod() == GET)
	{
		mResponseStream << "Content-Length: " << body.str().size();
		mResponseStream << "\r\n\r\n" << body.str();
	}
	mResponse = mResponseStream.str();
	mResponseReady = true;
}

// TODO: optimise this, use something like the stat() function instead of
// loading the entire file into memory
void ResponseBuilder::buildPartialResponse(RouteResult &route)
{
	std::ostringstream body;
	std::ifstream	   requestFile(route.filePath.c_str());

	// if the requested file could not be opened
	if (requestFile.good() == false)
	{
		buildSimpleResponse(500, "The requested file could not be accessed");
		return;
	}
	body << requestFile.rdbuf();
	mStatus = route.status;
	mResponseStream << "HTTP/1.1 " << mStatus << "\r\n";
	addCookies();
	addConnectionField(route.keepAlive);
	mResponseStream << "Content-Type: " << MimeTypes::getInstance()->getType(route.filePath)
					<< "\r\nAccept-Ranges: bytes\r\n";

	// if request doesn't give a max number, set it to end of the file
	if (route.partialLength == 0)
		route.partialLength = body.str().size() - 1;

	std::string partialBody = body.str().substr(route.partialOffset, route.partialLength + 1);

	LOG_DEBUG("response body size: " + numToString(partialBody.size()));
	mResponseStream << "Content-Range: bytes " << route.partialOffset << "-"
					<< route.partialLength + route.partialOffset << "/" << body.str().size();
	mResponseStream << "\r\nContent-Length: " << route.partialLength + 1;
	mResponseStream << "\r\n\r\n" << partialBody;

	mResponse = mResponseStream.str();
	mResponseReady = true;
}

// TODO: add a function that writes the name of the error code based on what error occurred next to
// the Error code, so for 404 it returns "Not Found", for 500 its "Internal Server Error"
// https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Status
void ResponseBuilder::buildErrorResponse(RouteResult &route)
{
	std::string errorPage = mConfig->errorPages[route.status];

	mStatus = route.status;
	mResponseStream << "HTTP/1.1 " << numToString(mStatus) << "\r\nContent-Type: text/html\r\n";
	addConnectionField(route.keepAlive);
	if (!errorPage.empty() && mParser->getMethod() != HEAD)
	{
		std::ostringstream body;

		errorPage = mConfig->root + mConfig->errorPages[route.status];
		std::ifstream inf(errorPage.c_str());

		body << inf.rdbuf();
		size_t size = body.str().size();
		mResponseStream << "Content-Length: " << numToString(size) << "\r\n\r\n" << body.str();
	}
	else if (mParser->getMethod() != HEAD)
	{
		mResponseStream << "Content-Length: " << numToString(81 + route.bodyMsg.size())
						<< "\r\n\r\n"
						<< "<!DOCTYPE html><html lang=\"en\"><h1>Webserv</h1><h2>Error: "
						<< numToString(mStatus) << "</h2><p> " << route.bodyMsg << "</p></html>";
	}
	else if (mParser->getMethod() == HEAD)
		mResponseStream << "Content-Length: 0\r\n\r\n";
	mResponse = mResponseStream.str();
	LOG_ERROR("ResponseBuilder: Error status: " + numToString(mStatus) + ": " + route.bodyMsg);
	LOG_DEBUG(mResponse);
	mResponseReady = true;
}

void ResponseBuilder::buildAutoIndex(RouteResult &route)
{
	std::string dirPath = route.filePath.substr(0, route.filePath.find_last_of('/'));
	std::string autoIndexBody = mAutoIndex.generatePage(dirPath, route.filePath);

	mStatus = route.status;
	mResponseStream << "HTTP/1.1 " << mStatus << "\r\n";
	addCookies();
	mResponseStream << "Content-Type: text/html\r\nAccept-Ranges: bytes\r\n";
	addConnectionField(route.keepAlive);
	if (route.type != RR_HEAD)
	{
		mResponseStream << "Content-Length: " << autoIndexBody.size();
		mResponseStream << "\r\n\r\n" << autoIndexBody;
	}
	else
	{
		mResponseStream << "Content-Length: 0\r\n\r\n";
	}
	mResponse = mResponseStream.str();
	mResponseReady = true;
}

void ResponseBuilder::buildRedirect(RouteResult &route)
{
	mStatus = route.status;
	mResponseStream << "HTTP/1.1 " << numToString(mStatus)
					<< "\r\nContent-Type: text/plain\r\nContent-Length: "
					<< numToString(34 + route.filePath.size()) << "\r\nLocation: " << route.filePath
					<< "\r\n\r\nMoved permanently. Redirecting to " << route.filePath;
	mResponse = mResponseStream.str();
	LOG_DEBUG(mResponse);
	mResponseReady = true;
}

void ResponseBuilder::addConnectionField(bool keepAlive)
{
	if (keepAlive == false)
		mResponseStream << "Connection: close\r\n";
}

void ResponseBuilder::buildSimpleResponse(int status, const std::string &msg)
{
	mStatus = status;
	mResponseStream << "HTTP/1.1 " << numToString(status)
					<< "\r\nContent-Type: text/plain\r\nContent-Length: " << numToString(msg.size())
					<< "\r\n\r\n"
					<< msg;
	mResponse = mResponseStream.str();
	LOG_DEBUG(mResponse);
	mResponseReady = true;
}

void ResponseBuilder::setSessionId(const std::string &id)
{
	mSessionId = id;
	mNewSession = true;
}
