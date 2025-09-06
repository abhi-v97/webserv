#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "CgiHandler.hpp"

/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

CgiHandler::CgiHandler(): m_PID(0), m_fd()
{
}

CgiHandler::CgiHandler(const CgiHandler &src)
	: m_header(src.m_header), m_PID(0), m_fd()
{
}

/*
** -------------------------------- DESTRUCTOR --------------------------------
*/

CgiHandler::~CgiHandler()
{
	this->m_header.clear();
}

/*
** --------------------------------- OVERLOAD ---------------------------------
*/

CgiHandler &CgiHandler::operator=(const CgiHandler &rhs)
{
	if (this != &rhs)
	{
		this->m_header = rhs.m_header;
		this->m_PID = rhs.m_PID;
	}
	return *this;
}

std::ostream &operator<<(std::ostream &outf, const CgiHandler &obj)
{
	// o << "Value = " << i.getValue();
	return outf;
}

/*
** --------------------------------- METHODS ----------------------------------
*/

// basic building blocks of running an external program
// simply calls python3 on the provided filename
// NEXT: fix up pipes and redirect, you're not resetting stdout after
// duping, this will not work for multiple clients
void CgiHandler::execute(std::string cgiName)
{
	std::string cgiFile = ("cgi-bin/" + cgiName);
	char *cwd = getcwd(NULL, 0);

	if (pipe(this->m_fd) < 0)
	{
		return;
	}

	this->m_PID = fork();

	if (this->m_PID == 0)
	{
		// child
		close(this->m_fd[0]);
		dup2(this->m_fd[1], STDOUT_FILENO);
		close(this->m_fd[1]);

		char *argv[3];
		argv[0] = (char *)"/usr/bin/python3";
		argv[1] = (char *)cgiFile.c_str();
		argv[2] = NULL;
		std::cerr << "starting execve" << std::endl;
		execve(argv[0], argv, NULL);
	}
	else
	{
		// parent
		waitpid(-1, NULL, 0);
		close(this->m_fd[1]);
	}
}

/*
** --------------------------------- ACCESSOR ---------------------------------
*/

int CgiHandler::getOutFd()
{
	return this->m_fd[0];
}

/* ************************************************************************** */
