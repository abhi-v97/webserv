#ifndef RESPONSEBUILDER_HPP
#define RESPONSEBUILDER_HPP

#include <cerrno>
#include <cstring>
#include <sstream>

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
	void		setStatus(int code);
	void		setErrorMessage(const std::string &error);
	int			getStatus();
	std::string getResponse();
	void		reset();

	size_t		mMin;
	size_t		mMax;
	size_t		mStatus;
	std::string mErrorMsg;

private:
	std::stringstream mResponse;
};

#endif
