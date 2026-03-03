#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <cstdlib>
#include <map>
#include <sched.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "IHandler.hpp"

class ClientHandler;
struct RouteResult;

class CgiHandler: public IHandler
{
public:
	CgiHandler(ClientHandler *client);
	~CgiHandler();

	bool execute(RouteResult &route);
	void setCgiResponse();
	int	 getFd() const;
	int	 getInFd() const;
	int	 getOutFd() const;
	void handleEvents(struct pollfd &pollStruct);
	bool getKeepAlive() const;

private:
	std::map<std::string, std::string> m_header;
	pid_t							   mPID;
	int								   mOutFd[2];
	int								   mInFd[2];
	ClientHandler					  *mClient;
	std::string						   mCgiBody;
	ssize_t							   mBodySize;
	ssize_t							   mInBytesWritten;
	std::vector<char *>				   mEnvp;
	std::vector<std::string>		   mEnvpStr;

	void buildArgs();
	void setCgiEnv(const std::string &field, const std::string &value);
};

#endif

/* ****************************************************** CGIHANDLER_H  */
