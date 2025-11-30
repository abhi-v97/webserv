#ifndef RESPONSEBUILDER_HPP
#define RESPONSEBUILDER_HPP

#include <cerrno>
#include <cstring>
#include <sstream>

class ResponseBuilder
{
public:
	ResponseBuilder();
	ResponseBuilder(const ResponseBuilder &src);
	~ResponseBuilder();

	ResponseBuilder &operator=(const ResponseBuilder &rhs);

	void reset();

	std::string buildResponse();
	bool readCgiResponse(int pipeOutFd);

	std::string getResponse();

private:
	std::stringstream mResponse;
};

#endif /* ************************************************* RESPONSEBUILDER_H  \
        */
