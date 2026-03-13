#include <algorithm>
#include <cctype>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

#include "ClientHandler.hpp"
#include "Logger.hpp"
#include "RequestParser.hpp"
#include "Utils.hpp"

/**
	\brief Constructor
*/
RequestParser::RequestParser()
	: mMethod(UNKNOWN), bodyToFile(false), mParsingFinished(false), mChunkedRequest(false),
	  bodyFd(-1), bodyExpected(0), bodyReceived(0), mHeaderEnd(0), mParsePos(0), mChunkSize(0),
	  mBodySize(0), mRequestCount(0), mState(HEADER)
{
}

/**
	\brief Parses a HTTP request using a state machine pattern

	\param requestBuffer HTTP request string
*/
bool RequestParser::parse(std::string &requestBuffer)
{
	std::string buffer;

	const size_t bufSize = requestBuffer.size();

	// check for new data
	if (mParsePos >= bufSize)
		return (true);

	// check if we have a new line to parse
	size_t lineEnd = requestBuffer.find("\r\n", mParsePos);
	if (lineEnd == std::string::npos)
		return (true);
	if (mState == HEADER)
	{
		for (; mParsePos < requestBuffer.size(); mParsePos++)
		{
			if (requestBuffer[mParsePos] != '\r' && requestBuffer[mParsePos] != '\n')
				break;
		}
		lineEnd = requestBuffer.find("\r\n", mParsePos);
		mRequestHeader = requestBuffer.substr(mParsePos, lineEnd - mParsePos);
		if (mRequestHeader.size() == 0)
			return (true);
		if (!parseHeader(mRequestHeader))
			return (false);
		mParsePos = lineEnd + 2;
	}
	if (mState == FIELD)
	{
		// now parse the header fields
		while (42)
		{
			size_t next = requestBuffer.find("\r\n", mParsePos);
			if (next == std::string::npos)
				return (true);
			std::string fieldLine = requestBuffer.substr(mParsePos, next - mParsePos);
			if (fieldLine.size() == 0)
			{
				mState = BODY;
				requestBuffer.erase(0, mParsePos + 2);
				mParsePos = 0;
				break;
			}
			else if (parseHeaderField(fieldLine) == false)
				return (false);
			mParsePos = next + 2;
		}
	}
	if (mState == BODY)
	{
		if (mMethod != POST)
		{
			mState = DONE;
			mParsingFinished = true;
			mRequestCount++;
			return (true);
		}
		if (parseBody(requestBuffer) == false)
			return (false);
	}
	// if (mMethod == POST)
	// {
	// 	size_t maxLength = mClient->mConfig->clientMaxBodySize;

	// 	if (maxLength && mBodySize > maxLength)
	// 		return (handleError(413, "Content too large."), false);
	// }
	if (mParsePos > 0)
	{
		requestBuffer.erase(0, mParsePos);
		mParsePos = 0;
		if (mState == BODY && !requestBuffer.empty())
			return (parse(requestBuffer));
	}
	return (true);
}

/**
	\brief Parses HTTP header request line

	\param header Header line substring from request
*/
bool RequestParser::parseHeader(const std::string &header)
{
	LOG_DEBUG("parsing request-line");
	// extract method
	int methodEnd = header.find_first_of(' ', 0);
	setMethod(header.substr(0, methodEnd));
	if (mMethod == UNKNOWN)
		return (handleError(400, "Unknown/invalid HTTP method provided"), false);

	// extract uri
	int uriEnd = header.find_first_of(' ', methodEnd + 1);
	if (uriEnd == std::string::npos)
		return (LOG_ERROR("parseHeader(): bad request-line"), false);
	mRequestUri = header.substr(methodEnd + 1, uriEnd - methodEnd - 1);
	if (validateUri(mRequestUri) == false)
		return (handleError(400, "parseHeader(): invalid URI format, must begin with / or http://"),
				false);

	// extract version
	int versionEnd = header.find_first_of('\r', uriEnd + 1);
	mHttpVersion = header.substr(uriEnd + 1, 8);
	if (mHttpVersion != "HTTP/1.1" && mHttpVersion != "HTTP/1.0")
		return (handleError(400, "Bad or invalid HTTP version"), false);

	// advance state
	mState = FIELD;
	return (true);
}

/**
	\brief Parses request header fields line by line

	\param buffer Single header field line
*/
bool RequestParser::parseHeaderField(std::string &buffer)
{
	size_t colonPos = buffer.find(':');
	if (colonPos == std::string::npos)
		return (true);

	std::string rawFieldName = buffer.substr(0, colonPos);
	size_t		firstChar = rawFieldName.find_first_not_of(" \t");
	size_t		lastChar = colonPos;
	if (firstChar == colonPos)
		return (handleError(400, "Invalid HTTP header field"), false);

	std::string fieldName = rawFieldName.substr(firstChar, lastChar - firstChar);
	if (fieldName.find_first_of(" \t") != std::string::npos)
		return (false);
	for (std::string::iterator it = fieldName.begin(); it != fieldName.end(); it++)
		*it = std::tolower(static_cast<unsigned char>(*it));

	size_t		fieldValueStart = buffer.find_first_not_of(" \t", colonPos + 1);
	size_t		fieldValueEnd = buffer.find_last_not_of("\r\n");
	std::string fieldValue = buffer.substr(fieldValueStart, fieldValueEnd - fieldValueStart + 1);
	if (fieldName == "cookie")
		mCookies += fieldValue + "; ";
	if (fieldValueStart == std::string::npos || fieldValueStart > fieldValueEnd)
		mHeaderField[fieldName] = "";
	else
		mHeaderField[fieldName] = fieldValue;
	return (true);
}

/**
	\brief Helper function used to convert hex string to uint for chunked encoding

	\param hex Hex number in std::string format
*/
unsigned int hexToInt(std::string &hex)
{
	unsigned int result = 0;
	unsigned int value = 0;
	// init base value to 1, representing 16^0

	for (std::string::iterator it = hex.begin(); it != hex.end(); it++)
	{
		if (*it >= '0' && *it <= '9')
			value = *it - '0';
		else if (*it >= 'A' && *it <= 'F')
			value = *it - 'A' + 10;
		else if (*it >= 'a' && *it <= 'f')
			value = *it - 'a' + 10;
		else
			break; // stop at invalid character

		result = result * 16 + value;
		// result = (result << 4) | value;
	}
	return (result);
}

/**
	\brief Parses chunked body

	\param request Request body string
*/
bool RequestParser::parseChunked(std::string &request)
{
	if (mChunkSize == 0)
	{
		size_t endPos = request.find("\r\n", mParsePos);
		if (endPos == std::string::npos)
			return (true);

		std::string hexSize = request.substr(mParsePos, endPos);
		mChunkSize = hexToInt(hexSize);
		if (mChunkSize == 0)
		{
			LOG_DEBUG("parsing chunked body done");
			endPos = request.find("\r\n", endPos + 2);
			if (endPos == std::string::npos)
				return (true);
			mParsingFinished = true;
			mState = DONE;
			mParsePos = endPos + 2;
			return (true);
		}
		mParsePos += endPos + 2;
	}
	size_t available = request.size() - mParsePos;
	size_t bodyStart = mParsePos;
	if (available)
	{
		size_t	toWrite = std::min(available, mChunkSize - bodyReceived);
		ssize_t bytesWritten = write(bodyFd, request.data() + bodyStart, toWrite);
		if (bytesWritten < 0)
			return (false);
		mBodySize += bytesWritten;
		bodyReceived += static_cast<size_t>(bytesWritten);
		mParsePos = bodyStart + bytesWritten;
		if (bodyReceived >= mChunkSize)
		{
			mChunkSize = 0;
			bodyReceived = 0;
			if (request.find("\r\n", mParsePos) == std::string::npos)
			{
				// couldn't find end of chunk, wait for more data.
				return (true);
			}
			mParsePos += 2;
		}
	}
	return (true);
}

/**
	\brief Parses through HTTP body

	\param request HTTP request string
*/
bool RequestParser::parseBody(std::string &request)
{
	// init vars, triggered on first call
	if (bodyExpected == 0 && mChunkedRequest == false)
	{
		bodyExpected = this->getContentLength();

		// no given content length
		if (bodyExpected == 0)
		{
			if (this->getEncoding() == true)
				mChunkedRequest = true;
			else
			{
				LOG_DEBUG("parsing body done");
				mParsingFinished = true;
				mState = DONE;
				mRequestCount++;
				return true;
			}
		}
		// else
		// {
		// 	size_t maxLength = mClient->mConfig->clientMaxBodySize;

		// 	if (maxLength && bodyExpected > maxLength)
		// 		return (handleError(413, "Content too large."), false);
		// }
		bodyReceived = 0;
		bodyToFile = false;
		bodyFd = -1;

		mTempPostFile = generateId() + ".tmp";
		int flags = O_WRONLY | O_CREAT | O_TRUNC;
		bodyFd = open(mTempPostFile.c_str(), flags, 0644);
		if (bodyFd < 0)
			return (handleError(500, "Failed to create a temporary file"), false);
		setNonBlockingFlag(bodyFd);
	}

	if (mChunkedRequest == true)
		return (parseChunked(request));

	size_t available = request.size();
	size_t bodyStart = mParsePos;
	if (available)
	{
		size_t	toWrite = std::min(available, bodyExpected - bodyReceived);
		ssize_t bytesWritten = write(bodyFd, request.data(), toWrite);
		if (bytesWritten < 0)
			return (handleError(500, "Failed to write to requested resource"), false);
		mBodySize += bytesWritten;
		bodyReceived += static_cast<size_t>(bytesWritten);
		mParsePos = bodyStart + bytesWritten;
	}
	if (bodyReceived >= bodyExpected)
	{
		mParsingFinished = true;
		mRequestCount++;
		mState = DONE;
	}
	return true;
}

/**
	\brief Helper function to set a parsing error

	\param statusCode HTTP status code of error
	\param errorMsg Error message, sent as response body
*/
void RequestParser::handleError(int statusCode, const std::string &errorMsg)
{
	LOG_ERROR("RequestParser: " + errorMsg);
	mResponse->buildSimpleResponse(statusCode, errorMsg);
	mParsingFinished = true;
}

/**
	\brief extremely minimalist URI parser.

	Simply looks for '/' as the first character or "http://" at the beginning.
*/
bool RequestParser::validateUri(const std::string &uri)
{
	std::string::const_iterator i = uri.begin();

	if (*i == '/')
		return (true);
	else if (uri.find("http://", 0, 7) != std::string::npos)
	{
		mRequestUri = mRequestUri.substr(6);
		return (true);
	}
	return (false);
}

//
/**
	\brief Returns false if HTTP version is 1.0 as it did not support persistent connections. Also
   returns false if client requested connection to be closed
*/
bool RequestParser::getKeepAliveRequest()
{
	if (*(mHttpVersion.rbegin()) == '0')
		return (false);
	if (mHeaderField.find("connection") != mHeaderField.end())
	{
		if (mHeaderField["connection"] == "close")
			return (false);
	}
	return (true);
}

/**
	\brief Returns false if HTTP version is 1.0 as it did not support chunked encoding. Also returns
   false if header field is not specified/has typos
*/
bool RequestParser::getEncoding()
{
	if (*(mHttpVersion.rbegin()) == '0')
		return (false);
	if (mHeaderField.find("transfer-encoding") != mHeaderField.end())
	{
		if (mHeaderField["transfer-encoding"] == "chunked")
			return (true);
	}
	return (false);
}

/**
	\brief Getter for temporary request body file
*/
const std::string &RequestParser::getBodyFile() const
{
	return (this->mTempPostFile);
}

/**
	\brief Looks for and returns body size from content length header field
*/
size_t RequestParser::getContentLength()
{
	size_t result;

	if (mHeaderField.find("content-length") != mHeaderField.end())
	{
		std::stringstream lengthStream(mHeaderField["content-length"]);
		lengthStream >> result;
		return result;
	}
	return (0);
}

/**
	\brief Resets variables in-between requests
*/
void RequestParser::reset()
{
	mMethod = UNKNOWN;
	bodyToFile = false;
	mParsingFinished = false;
	mChunkedRequest = false;
	bodyFd = -1;
	bodyExpected = 0;
	bodyReceived = 0;
	mHeaderEnd = 0;
	mParsePos = 0;
	mChunkSize = 0;
	mBodySize = 0;
	mRequestUri.clear();
	mHttpVersion.clear();
	mHeaderField.clear();
	mRequestHeader.clear();
	mTempPostFile.clear();
	mCookies.clear();
	mState = HEADER;
}

/**
	\brief Getter for request header, used to log HTTP request
*/
std::string &RequestParser::getRequestHeader()
{
	return (this->mRequestHeader);
}

/**
	\brief Getter for request method
*/
RequestMethod RequestParser::getMethod()
{
	return (this->mMethod);
}

/**
	\brief Getter for request uri
*/
std::string &RequestParser::getUri()
{
	return (this->mRequestUri);
}

/**
	\brief Getter for request header fields, given as a map
*/
std::map<std::string, std::string> &RequestParser::getHeaders()
{
	return (this->mHeaderField);
}

/**
	\brief Getter for cookies found in request
*/
std::string &RequestParser::getCookies()
{
	return (this->mCookies);
}

/**
	\brief returns true if parsing is finished
*/
bool RequestParser::getParsingFinished() const
{
	return this->mParsingFinished;
}

/**
	\brief Setter for request method
*/
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
	else if (methodStr == "DELETE")
	{
		mMethod = DELETE;
	}
}

/**
	\brief Getter for total request count
*/
int RequestParser::getRequestCount()
{
	return (this->mRequestCount);
}
