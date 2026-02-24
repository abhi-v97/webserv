#ifndef RESPONSEBUILDER_HPP
#define RESPONSEBUILDER_HPP

#include <cerrno>
#include <cstring>
#include <sstream>
#include <string>

#include "configParser.hpp"

class RequestParser;
struct RouteResult;

// TODO: instead of ClientHandler accessing the response through mResponse, maybe just pass
// mResponseStream instead?

// TODO: instead of each ClientHandler having a response object, just make one in Dispatch and have
// each ClientHandler access it since its mostly stateless

/**
	\class ResponseBuilder

	Class that generates a HTTP response based on validity of the request
*/
class ResponseBuilder
{
public:
	ResponseBuilder();
	~ResponseBuilder();

	void		buildResponse(const std::string &uri);
	void		buildPartialResponse(RouteResult &out);
	void		buildErrorResponse(RouteResult &route);
	void		buildSimpleResponse(int status, const std::string &msg);
	bool		readCgiResponse(int pipeOutFd);
	void		setError(int code, const std::string &error);
	void		setSessionId(const std::string &id);
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
