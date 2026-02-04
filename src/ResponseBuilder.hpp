#ifndef RESPONSEBUILDER_HPP
#define RESPONSEBUILDER_HPP

#include <cerrno>
#include <cstring>
#include <sstream>
#include "configParser.hpp"

class RequestParser;

class ResponseBuilder
{
public:
	ResponseBuilder();
	~ResponseBuilder();

	std::string buildResponse(const std::string &uri);
	std::string buildErrorResponse();
	bool		readCgiResponse(int pipeOutFd);
	void		parseRangeHeader(RequestParser &parser);
	void		setError(int code, const std::string error);
	int			getStatus();
	std::string getResponse();
	void		reset();

	size_t		   mMin;
	size_t		   mMax;
	size_t		   mStatus;
	std::string	   mErrorMsg;
	RequestParser *mParser;
	ServerConfig *mConfig;

private:
	std::ostringstream mResponse;

	void addCookies();
};

#endif
