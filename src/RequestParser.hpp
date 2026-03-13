#pragma once

#include <cstddef>
#include <map>
#include <string>

#include "ResponseBuilder.hpp"

// TODO: separate RequestParser into a stateless object, and separate the
// request information into a Request class object
// Client handler maintains Request, Dispatch maintains RequestParser

class ClientHandler;

enum RequestMethod
{
	GET,
	HEAD,
	POST,
	DELETE,
	UNKNOWN,
};

enum RequestState
{
	HEADER,
	FIELD,
	BODY,
	DONE
};

/**
	\class RequestParser

	Parses and validates a given HTTP request string. Holds request information that can be accessed
   by ClientHandler and ResponseBuilder.
*/
class RequestParser
{
public:
	RequestParser();

	std::map<std::string, std::string> &getHeaders();
	std::string						   &getRequestHeader();
	RequestMethod						getMethod();
	std::string						   &getUri();
	size_t								getContentLength();
	bool								getParsingFinished() const;
	bool								getKeepAliveRequest();
	const std::string				   &getBodyFile() const;
	int									getRequestCount();
	bool								parse(std::string &requestBuffer);
	bool								parseBody(std::string &request);
	std::string						   &getCookies();
	void								reset();

	ResponseBuilder *mResponse;
	size_t			 mBodySize;
	ClientHandler	*mClient;
	std::string		 mRequestUri;

private:
	bool parseHeader(const std::string &header);
	void setMethod(const std::string &method);
	bool validateUri(const std::string &uri);
	bool parseHeaderField(std::string &buffer);
	bool getEncoding();
	bool parseChunked(std::string &request);
	void handleError(int code, const std::string &errorMsg);

	RequestMethod					   mMethod;
	std::string						   mHttpVersion;
	std::string						   mRequestHeader;
	std::string						   mTempPostFile;
	std::string						   mCookies;
	std::map<std::string, std::string> mHeaderField;
	bool							   bodyToFile;
	bool							   mParsingFinished;
	bool							   mChunkedRequest;
	int								   bodyFd;
	size_t							   bodyExpected;
	size_t							   bodyReceived;
	size_t							   mHeaderEnd;
	size_t							   mParsePos;
	size_t							   mChunkSize;
	int								   mRequestCount;
	RequestState					   mState;
};

// References:
// max client request header size from nginx
// https://nginx.org/en/docs/http/ngx_http_core_module.html#large_client_header_buffers
