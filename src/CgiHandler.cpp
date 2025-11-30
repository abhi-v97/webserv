#include <csignal>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "CgiHandler.hpp"
#include "Server/Reactor.hpp"

/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

CgiHandler::CgiHandler(): m_PID(0), m_fd()
{
}

CgiHandler::CgiHandler(const CgiHandler &src): m_header(src.m_header), m_PID(0), m_fd()
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
** --------------------------------- METHODS ----------------------------------
*/

// basic building blocks of running an external program
// simply calls python3 on the provided filename
// NEXT: fix up pipes and redirect, you're not resetting stdout after
// duping, this will not work for multiple clients
void CgiHandler::execute(std::string cgiName)
{
	std::string cgiFile = ("cgi-bin/" + cgiName);

	if (pipe(this->m_fd) < 0)
	{
		return;
	}

	this->m_PID = fork();

	if (this->m_PID == 0)
	{
		// child

		// set sigint to default action in child process
		std::signal(SIGINT, SIG_DFL);

		close(this->m_fd[0]);
		dup2(this->m_fd[1], STDOUT_FILENO);
		close(this->m_fd[1]);

		char *argv[3];
		argv[0] = (char *) "/usr/bin/python3";
		argv[1] = (char *) cgiFile.c_str();
		argv[2] = NULL;
		std::cerr << "starting execve" << std::endl;
		execve(argv[0], argv, NULL);
	}
	else
	{
		// parent

		// ignore sigint while child process is running
		std::signal(SIGINT, SIG_IGN);

		// TODO: test this, make a script that sleep for 3s and then executes
		waitpid(-1, NULL, WNOHANG);

		// reset sigint after cgi process is over
		std::signal(SIGINT, signalHandler);

		close(this->m_fd[1]);
	}
}

void CgiHandler::buildArgs()
{
}

/*
** --------------------------------- ACCESSOR ---------------------------------
*/

int CgiHandler::getOutFd()
{
	return this->m_fd[0];
}

void CgiHandler::setCgiType(CgiType type)
{
	this->mType = type;
}

/* ************************************************************************** */
