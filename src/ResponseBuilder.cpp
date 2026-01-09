#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unistd.h>

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

void ResponseBuilder::reset()
{
	mResponse.clear();
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
	std::string file = "www";

	file.append(uri);

	std::ifstream	   requestFile(file.c_str());
	std::ostringstream response;
	std::ostringstream body;

	body << requestFile.rdbuf();
	response << "HTTP/1.1 " << mStatus
			 << " 0K\r\nContent-Type: " << MimeTypes::getInstance()->getType(uri)
			 << "\r\nAccept-Ranges: bytes\r\n";

	if (mMax == 0)
		mMax = body.str().size() - 1;

	if (mStatus == 206)
	{
		std::string newbody = body.str().substr(mMin, mMax - mMin + 1);
		response << "Content-Range: bytes " << mMin << "-" << mMax << "/" << body.str().size();
		response << "\r\nContent-Length: " << mMax - mMin + 1;
		response << "\r\n\r\n" << newbody;
	}
	else
	{
		response << "Content-Length: " << body.str().size();
		response << "\r\n\r\n" << body.str();
	}
	return response.str();
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
	mResponse << "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: " << body.str().size()
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
