#ifndef RESPONSEBUILDER_HPP
#define RESPONSEBUILDER_HPP

#include <cerrno>
#include <cstring>
#include <iostream>

class ResponseBuilder
{
public:
	ResponseBuilder();
	ResponseBuilder(const ResponseBuilder &src);
	~ResponseBuilder();

	ResponseBuilder &operator=(const ResponseBuilder &rhs);

	std::string buildResponse();
	std::string buildCgiResponse(int pipeOutFd);

private:
};

std::ostream &operator<<(std::ostream &o, const ResponseBuilder &i);

#endif /* ************************************************* RESPONSEBUILDER_H  \
        */
