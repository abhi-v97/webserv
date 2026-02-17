#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <cstdlib>
#include <iostream>
#include <map>
#include <sched.h>
#include <string>
#include <unistd.h>

#include "IHandler.hpp"
#include "configParser.hpp"

class WriteHandler;

enum CgiType
{
	IDK,
	PYTHON,
	SHELL,
};

class CgiHandler: public IHandler
{
public:
	CgiHandler(WriteHandler *client);
	CgiHandler(const CgiHandler &src);
	~CgiHandler();

	bool execute(std::string cgiName);
	void setCgiType(CgiType type);
	void setCgiResponse();
	int	 getFd() const;
	void handleEvents(struct pollfd &pollStruct);
	bool getKeepAlive() const;

private:
	std::map<std::string, std::string> m_header;
	pid_t							   m_PID;
	int								   m_fd[2];
	CgiType							   mType;
	WriteHandler					  *mClient;
	std::string						   mCgiBody;
	ssize_t							   mBodySize;

	void buildArgs();
};

std::ostream &operator<<(std::ostream &outf, const CgiHandler &obj);

#endif

/* ****************************************************** CGIHANDLER_H  */
