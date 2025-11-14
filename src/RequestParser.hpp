#pragma once

#include <iostream>
#include <map>

enum RequestMethod
{
	GET,
	HEAD,
	POST,
	UNKNOWN,
};

class RequestParser
{
public:
	RequestParser();
	~RequestParser();

	std::map<std::string, std::string> &getHeaders();
	void getRequestHeader();
	RequestMethod getMethod();
	std::string &getUri();
	size_t getContentLength();
	bool getParsingFinished();

	void setHeaderEnd(const size_t &headerEnd);
	void setParsingFinished(const bool &status);

	void parse(const std::string &requestBuffer);
	bool parseBody(std::string &request);

	void reset();

private:
	void parseHeader(const std::string &header);
	void setMethod(const std::string &method);
	// void initHeaderFields();

	RequestMethod mMethod;
	std::string mRequestUri;
	std::string mHttpVersion;
	std::map<std::string, std::string> mHeaderField;
	bool bodyToFile;
	bool parsingFinished;
	int bodyFd;
	size_t bodyExpected;
	size_t bodyReceived;
	size_t mHeaderEnd;
};

// References:
// max client request header size from nginx
// https://nginx.org/en/docs/http/ngx_http_core_module.html#large_client_header_buffers
