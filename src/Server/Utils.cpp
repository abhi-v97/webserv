#include <fcntl.h>
#include <fstream>
#include <vector>

#include "Utils.hpp"

/**
	Helper function which adds the non-blocking flag to the listen socket.

	\param socketFd
*/
bool setNonBlockingFlag(int socketFd)
{
	int flags = fcntl(socketFd, F_GETFL, 0);
	if (flags == -1)
	{
		std::cerr << "get flag fcntl operation failed" << std::endl;
		return false;
	}
	int status = fcntl(socketFd, F_SETFL, flags | O_NONBLOCK);
	if (status == -1)
	{
		std::cerr << "set flag fcntl operation failed" << std::endl;
		return false;
	}
	return true;
}

std::string generateId()
{
	unsigned char temp[12];

	std::ifstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
	if (!urandom)
		return "";

	urandom.read((char *) temp, 12);
	urandom.close();

	std::string sessionId;
	char		hexBuffer[3];
	for (size_t i = 0; i < 6; ++i)
	{
		sprintf(hexBuffer, "%02x", temp[i]);
		sessionId += hexBuffer;
	}
	return sessionId;
}
