#include <csignal>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "CgiHandler.hpp"
#include "ClientHandler.hpp"
#include "Dispatcher.hpp"

#define BUFFER_SIZE 4096

/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

CgiHandler::CgiHandler(ClientHandler *client): m_PID(0), m_fd(), mClient(client)
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
bool CgiHandler::execute(std::string cgiName)
{
	std::string cgiFile = ("cgi-bin/" + cgiName);

	if (pipe(this->m_fd) < 0)
		return (false);

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
		std::exit(127);
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
	return (true);
}

void CgiHandler::handleEvents(struct pollfd &pollStruct)
{
	char buffer[BUFFER_SIZE];
	memset(buffer, 0, BUFFER_SIZE);

	ssize_t len = read(m_fd[0], buffer, BUFFER_SIZE);

	// returns true if len isn't positive, meaning read is finished
	if (len <= 0)
	{
		mClient->setCgiReady(true);
		setCgiResponse();
		return;
	}
	mResponse << buffer;
}

void CgiHandler::setCgiResponse()
{
	std::string &clientResponse = mClient->getResponse();

	clientResponse.append("HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: ");
}

void CgiHandler::buildArgs()
{
}

/*
** --------------------------------- ACCESSOR ---------------------------------
*/

int CgiHandler::getFd() const
{
	return this->m_fd[0];
}

void CgiHandler::setCgiType(CgiType type)
{
	this->mType = type;
}

bool CgiHandler::getKeepAlive() const
{
	return (this->mKeepAlive);
}

/* ************************************************************************** */
