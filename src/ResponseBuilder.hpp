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

	void reset();

	std::string buildResponse(const std::string &uri);
	bool		readCgiResponse(int pipeOutFd);
	void		parseRangeHeader(RequestParser &parser);

	std::string getResponse();
	size_t		mMin;
	size_t		mMax;
	size_t		mStatus;

private:
	std::stringstream mResponse;
};

#endif /* ************************************************* RESPONSEBUILDER_H                      \
		*/
