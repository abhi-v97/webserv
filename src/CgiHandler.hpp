#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <iostream>
#include <map>
#include <sched.h>
#include <string>

enum CgiType
{
	IDK,
	PYTHON,
	SHELL,
};

class CgiHandler
{
public:
	CgiHandler();
	CgiHandler(const CgiHandler &src);
	~CgiHandler();

	void execute(std::string cgiName);

	int	 getOutFd();
	void setCgiType(CgiType type);

private:
	std::map<std::string, std::string> m_header;
	pid_t							   m_PID;
	int								   m_fd[2];
	CgiType							   mType;

	void buildArgs();
};

#endif

/* ****************************************************** CGIHANDLER_H  */
