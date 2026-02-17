#ifndef RESPONSEBUILDER_HPP
#define RESPONSEBUILDER_HPP

#include <cerrno>
#include <cstring>
#include <sstream>
#include <string>

#include "configParser.hpp"

class RequestParser;

// TODO: instead of ReadHandler accessing the response through mResponse, maybe just pass
// mResponseStream instead?

class ResponseBuilder
{
public:
	ResponseBuilder();
	~ResponseBuilder();

	void		buildResponse(const std::string &uri);
	void		buildErrorResponse(int code, const std::string &errorMsg);
	bool		readCgiResponse(int pipeOutFd);
	void		parseRangeHeader(RequestParser &parser);
	void		setError(int code, const std::string &error);
	void		setSessionId(const std::string &id);
	int			getStatus();
	std::string getResponse();
	void		reset();

	bool		   mResponseReady;
	bool		   mNewSession;
	size_t		   mMin;
	size_t		   mMax;
	size_t		   mStatus;
	std::string	   mResponse;
	std::string	   mSessionId;
	RequestParser *mParser;
	ServerConfig  *mConfig;

private:
	std::ostringstream mResponseStream;

	void addCookies();
};

#endif
