#pragma once

#include <cstddef>
#include <iostream>
#include <map>
#include <string>

enum RequestMethod
{
	GET,
	HEAD,
	POST,
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
	void								getRequestHeader();
	RequestMethod						getMethod();
	std::string						   &getUri();
	size_t								getContentLength();
	bool								getParsingFinished() const;
	int									keepAlive();

	void setHeaderEnd(const size_t &headerEnd);

	bool parse(std::string &requestBuffer);
	bool parseBody(std::string &request);

	void reset();

private:
	bool parseHeader(const std::string &header);
	void setMethod(const std::string &method);
	bool validateUri(const std::string &uri);
	bool parseHeaderField(std::string &buffer);

	RequestMethod					   mMethod;
	std::string						   mRequestUri;
	std::string						   mHttpVersion;
	std::string						   mRequestHeader;
	std::map<std::string, std::string> mHeaderField;
	bool							   bodyToFile;
	bool							   parsingFinished;
	int								   bodyFd;
	size_t							   bodyExpected;
	size_t							   bodyReceived;
	size_t							   mHeaderEnd;
	size_t							   mParsePos;
	int								   mClientNum;
	int								   mStatusCode;
	RequestState					   mState;
};

// References:
// max client request header size from nginx
// https://nginx.org/en/docs/http/ngx_http_core_module.html#large_client_header_buffers
