#include <fstream>
#include <sstream>
#include <unistd.h>

#include "ResponseBuilder.hpp"

# define BUFFER_SIZE 4096

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

// simple code that grabs everything in a html file and appends it as "body" to
// a stanard header line
std::string ResponseBuilder::buildResponse()
{
	std::ifstream htmlFile("test.html");
	std::ostringstream response;
	std::ostringstream body;

	body << htmlFile.rdbuf();
	response << "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: "
			 << body.str().size() << "\n\n"
			 << body.str();
	return response.str();
}

std::string ResponseBuilder::buildCgiResponse(int pipeOutFd)
{
	std::ostringstream body;
	std::ostringstream response;

	char buffer[BUFFER_SIZE];
	ssize_t len = read(pipeOutFd, buffer, BUFFER_SIZE);

	while (len > 0)
	{
		std::cout << buffer << std::endl;
		body << buffer;
		len = read(pipeOutFd, buffer, BUFFER_SIZE);
	}
	response << "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: "
			 << body.str().size() << "\n\n"
			 << body.str();
	return response.str();
}

std::ostream &operator<<(std::ostream &outf, const ResponseBuilder &obj)
{
	// o << "Value = " << i.getValue();
	(void) obj;
	return outf;
}

/*
** --------------------------------- METHODS ----------------------------------
*/

/*
** --------------------------------- ACCESSOR ---------------------------------
*/

/* ************************************************************************** */
