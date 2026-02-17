#pragma once

#include <cstddef>
#include <iostream>
#include <map>
#include <string>

#include "ResponseBuilder.hpp"

#define INVALID_METHOD "Unknown method provided in request header"

class ReadHandler;

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

class RequestParser
{
public:
	RequestParser();
	~RequestParser();

	std::map<std::string, std::string> &getHeaders();
	std::string						   &getRequestHeader();
	RequestMethod						getMethod();
	std::string						   &getUri();
	size_t								getContentLength();
	bool								getParsingFinished() const;
	bool								getKeepAliveRequest();
	std::string const &getBodyFile() const;
	int									getRequestCount();
	void								setHeaderEnd(const size_t &headerEnd);
	bool								parse(std::string &requestBuffer);
	bool								parseBody(std::string &request);
	std::string						   &getCookies();
	void								reset();
	ResponseBuilder					   *mResponse;
	ReadHandler					   *mClient;

private:
	bool parseHeader(const std::string &header);
	void setMethod(const std::string &method);
	bool validateUri(const std::string &uri);
	bool parseHeaderField(std::string &buffer);
	bool getEncoding();
	bool parseChunked(std::string &request);
	void handleError(int code, const std::string &errorMsg);

	RequestMethod					   mMethod;
	std::string						   mRequestUri;
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
	int								   mClientNum;
	int								   mStatusCode;
	int								   mRequestCount;
	RequestState					   mState;
};

// References:
// max client request header size from nginx
// https://nginx.org/en/docs/http/ngx_http_core_module.html#large_client_header_buffers
