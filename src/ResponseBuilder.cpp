#include <cerrno>
#include <fstream>
#include <iostream>
#include <unistd.h>

#include "MimeTypes.hpp"
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

// simple response to a GET request
// grabs the file pointed to by uri and passes it along in its entirety
// now supports the content-type attribute, provided a matching extension exists in mime.types file
// if not, the file is treated as a plain text file
std::string ResponseBuilder::buildResponse(const std::string &uri)
{
	std::string file = "www";

	file.append(uri);

	std::ifstream	   requestFile(file.c_str());
	std::ostringstream response;
	std::ostringstream body;

	body << requestFile.rdbuf();
	response << "HTTP/1.1 200 OK\r\nContent-Type: " << MimeTypes::getInstance()->getType(uri)
			 << "\r\nContent-Length: " << body.str().size() << "\r\n\r\n"
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
