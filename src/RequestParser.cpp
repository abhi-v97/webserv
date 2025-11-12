#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

#include "RequestParser.hpp"
#include "WebServer.hpp" // TODO: included for the global vars, probably unneccessary so remove later

RequestParser::RequestParser()
	: mMethod(UNKNOWN), bodyToFile(false), parsingFinished(false), bodyFd(-1),
	  bodyExpected(0), bodyReceived(0), mHeaderEnd(0)
{
}

RequestParser::~RequestParser()
{
}

void RequestParser::reset()
{
	mMethod = UNKNOWN;
	bodyToFile = false;
	parsingFinished = false;
	bodyFd = -1;
	bodyExpected = 0;
	bodyReceived = 0;
	mHeaderEnd = 0;
	mRequestUri.clear();
	mHttpVersion.clear();
	mHeaderField.clear();
}

void RequestParser::setMethod(const std::string &methodStr)
{
	if (methodStr == "GET")
	{
		mMethod = GET;
	}
	else if (methodStr == "HEAD")
	{
		mMethod = HEAD;
	}
	else if (methodStr == "POST")
	{
		mMethod = POST;
	}
}

size_t RequestParser::getContentLength()
{
	size_t result;
	if (mMethod == GET || mMethod == HEAD)
	{
		return 0;
	}
	else if (mMethod == POST)
	{
		if (mHeaderField.find("Content-Length") != mHeaderField.end())
		{
			std::stringstream lengthStream(mHeaderField["Content-Length"]);
			lengthStream >> result;
			if (result > MAX_BODY_IN_MEM)
			{
				// TODO: content length is too long, idk, throw an exception or
				// something
			}
			return result;
		}
	}
	return 0;
}

RequestMethod RequestParser::getMethod()
{
	return this->mMethod;
}

std::string &RequestParser::getUri()
{
	return this->mRequestUri;
}

std::map<std::string, std::string> &RequestParser::getHeaders()
{
	return this->mHeaderField;
}

bool RequestParser::getParsingFinished()
{
	return this->parsingFinished;
}

void RequestParser::parseHeader(const std::string &header)
{
	std::istringstream headerStream(header);
	std::string methodStr;

	headerStream >> methodStr;
	setMethod(methodStr);
	headerStream >> mRequestUri;
	headerStream >> mHttpVersion;
}

void RequestParser::setHeaderEnd(const size_t &headerEnd)
{
	this->mHeaderEnd = headerEnd;
}

void RequestParser::setParsingFinished(const bool &status)
{
	this->parsingFinished = status;
}

void RequestParser::parse(const std::string &requestBuffer)
{
	std::stringstream inf(requestBuffer);
	std::string buffer;

	// parse the header line on its own
	std::getline(inf, buffer);
	if (buffer.empty())
	{
		throw(std::runtime_error("Error: requestParser: file not found"));
	}
	parseHeader(buffer);

	// now parse the header fields
	for (; std::getline(inf, buffer);)
	{
		if (buffer.empty() || buffer == "\r")
		{
			break;
		}

		size_t colonPos = buffer.find(':');
		if (colonPos == std::string::npos)
		{
			throw std::runtime_error(
				"Header line is missing a colon separator.");
		}

		std::string rawFieldName = buffer.substr(0, colonPos);
		size_t firstChar = rawFieldName.find_first_not_of(" \t");
		size_t lastChar = rawFieldName.find_last_not_of(" \t");
		if (firstChar == std::string::npos)
		{
			throw std::runtime_error("Field name is empty or only whitespace.");
		}

		std::string fieldName =
			rawFieldName.substr(firstChar, lastChar - firstChar + 1);
		if (fieldName.find_first_of(" \t") != std::string::npos)
		{
			throw std::runtime_error("Field name contains whitespace");
		}
		// std::cout << "Field name: " << fieldName << "." << std::endl;

		size_t fieldValueStart = buffer.find_first_not_of(" \t", colonPos + 1);
		size_t fieldValueEnd = buffer.find_last_not_of("\r\n");
		std::string fieldValue;
		if (fieldValueStart == std::string::npos)
		{
			fieldValue = "";
		}
		else
		{
			fieldValue = buffer.substr(fieldValueStart,
			                           fieldValueEnd - fieldValueStart + 1);
		}
		mHeaderField[fieldName] = fieldValue;
		// std::cout << fieldName << " = " << fieldValue << "." << std::endl;
		// yet another TODO: header fields names are case insensitive
		// apparently.
	}
}

bool RequestParser::parseBody(std::string &request)
{
	size_t contentLength = this->getContentLength();

	// init vars, triggered on first call
	if (bodyExpected == 0 && contentLength > 0)
	{
		bodyExpected = contentLength;
		bodyReceived = 0;
		bodyToFile = false;
		bodyFd = -1;
	}

	// no expected body, set parsing as finished
	if (bodyExpected == 0)
	{
		parsingFinished = true;
		return true;
	}

	// size of body, which is total request size - header size and CRLF
	size_t bodyBufferSize = request.size() - mHeaderEnd - 4;

	// check if body size is greater than what we can hold in memory
	if (!bodyToFile && bodyReceived + bodyBufferSize > MAX_BODY_IN_MEM)
	{
		std::string tempFile;

		tempFile = "client_" + numToString(bodyFd) + ".tmp";
		int flags = O_WRONLY | O_CREAT | O_TRUNC;
		int fd = open(tempFile.c_str(), flags, 0644);
		setNonBlockingFlag(fd);

		if (fd > 0)
		{
			bodyFd = fd;
			bodyToFile = true;
		}
		else
		{
			// TODO: handle this error better
			// wrap it in an exception, or simply close the client
			// connection and return
			std::cerr << strerror(errno) << std::endl;
		}
	}

	// write data to file or buffer
	if (bodyToFile)
	{
		ssize_t bytesWritten =
			write(bodyFd, request.data() + mHeaderEnd + 4, bodyBufferSize);
		if (bytesWritten > 0)
		{
			bodyReceived += bytesWritten;
		}
	}
	else
	{
		bodyReceived += bodyBufferSize;
	}

	// check if all data has been parsed
	if (bodyReceived >= bodyExpected)
	{
		parsingFinished = true;
		request.erase(0, mHeaderEnd + 4); // remove header plus CRLF

		if (!bodyToFile)
		{
			request.erase(0, mHeaderEnd + 4 + bodyExpected);
		}
		else
		{
			close(bodyFd);
			bodyFd = -1;
		}

		bodyExpected = 0;
		bodyReceived = 0;
		bodyToFile = false;
		bodyFd = -1;

		return true;
	}
	return false;
}
