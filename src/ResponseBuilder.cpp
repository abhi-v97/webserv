#include <cerrno>
#include <fstream>
#include <unistd.h>

#include "ResponseBuilder.hpp"

#define BUFFER_SIZE 4096

/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

ResponseBuilder::ResponseBuilder()
{
}

ResponseBuilder::ResponseBuilder(const ResponseBuilder &src)
{
}

/*
** -------------------------------- DESTRUCTOR --------------------------------
*/

ResponseBuilder::~ResponseBuilder()
{
}

/*
** --------------------------------- OVERLOAD ---------------------------------
*/

ResponseBuilder &ResponseBuilder::operator=(const ResponseBuilder &rhs)
{
	// if ( this != &rhs )
	//{
	// this->_value = rhs.getValue();
	//}
	return *this;
}

std::string ResponseBuilder::getResponse()
{
	return mResponse.str();
}

void ResponseBuilder::reset()
{
	mResponse.clear();
}

// simple code that grabs everything in a html file and appends it as "body" to
// a stanard header line
std::string ResponseBuilder::buildResponse()
{
	std::ifstream	   htmlFile("test.html");
	std::ostringstream response;
	std::ostringstream body;

	body << htmlFile.rdbuf();
	response << "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: " << body.str().size()
			 << "\n\n"
			 << body.str();
	return response.str();
}

bool ResponseBuilder::readCgiResponse(int pipeOutFd)
{
	std::ostringstream body;

	char buffer[BUFFER_SIZE];
	memset(buffer, 0, BUFFER_SIZE);

	ssize_t len = read(pipeOutFd, buffer, BUFFER_SIZE);
	while (len > 0)
	{
		body << buffer;
		len = read(pipeOutFd, buffer, BUFFER_SIZE);
	}
	mResponse << "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: " << body.str().size()
			  << "\n\n"
			  << body.str();
	std::cout << mResponse.str() << std::endl;

	// returns true if len isn't positive, meaning read is finished
	return len <= 0;
}

/*
** --------------------------------- METHODS ----------------------------------
*/

/*
** --------------------------------- ACCESSOR ---------------------------------
*/

/* ************************************************************************** */
