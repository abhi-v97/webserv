#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "CgiHandler.hpp"
#include "ClientHandler.hpp"
#include "Dispatcher.hpp"
#include "Logger.hpp"
#include "RequestManager.hpp"
#include "Utils.hpp"

#define BUFFER_SIZE 4096

/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

CgiHandler::CgiHandler(ClientHandler *client)
	: mPID(0), mOutFd(), mClient(client), mCgiBody(), mBodySize(0)
{
}

CgiHandler::~CgiHandler()
{
	if (mPID > 0)
	{
		int	  status = 0;
		pid_t pid;

		// -mPID means kill entire process group, not just the single one
		kill(-mPID, SIGTERM);

		// grace period, wait for it to close normally
		int waitMs = 0;
		while (waitMs < 500)
		{
			pid = waitpid(mPID, &status, WNOHANG);
			if (pid == mPID)
				break;
			usleep(50 * 1000);
			waitMs += 50;
		}

		// if it didn't exit, force kill
		if (pid != mPID)
		{
			kill(-mPID, SIGKILL);
			waitpid(mPID, &status, 0);
		}
		mPID = 0;
	}
	if (mOutFd[0] >= 0)
	{
		close(mOutFd[0]);
		mOutFd[0] = -1;
	}
	if (mOutFd[1] >= 0)
	{
		close(mOutFd[1]);
		mOutFd[1] = -1;
	}
	if (mInFd[0] >= 0)
	{
		close(mInFd[0]);
		mInFd[0] = -1;
	}
	if (mInFd[1] >= 0)
	{
		close(mInFd[1]);
		mInFd[1] = -1;
	}
}

/*
** --------------------------------- METHODS ----------------------------------
*/

void CgiHandler::setCgiEnv(const std::string &field, const std::string &value)
{
	std::string item = field + "=" + value;
	mEnvpStr.push_back(item);

	char *c = strdup(mEnvpStr.back().c_str());
	mEnvp.push_back(c);
}

bool CgiHandler::execute(RouteResult &route)
{
	if (pipe(this->mInFd) < 0)
		return (false);
	if (pipe(this->mOutFd) < 0)
		return (close(this->mInFd[0]), close(this->mInFd[1]), false);

	this->mPID = fork();
	if (this->mPID == 0)
	{
		// child

		// set process group so shutting it down also closes any sub-processes
		setpgid(0, 0);

		// set sigint to default action in child process
		std::signal(SIGINT, SIG_DFL);

		close(this->mOutFd[0]);
		dup2(this->mOutFd[1], STDOUT_FILENO);
		close(this->mOutFd[1]);

		close(this->mInFd[1]);
		dup2(this->mInFd[0], STDIN_FILENO);
		close(this->mInFd[0]);

		// TODO: create env variables

		struct stat statbuf;

		if (stat(mClient->getRequestBody().c_str(), &statbuf) == 0)
		{
			long size = statbuf.st_size;

			if (size < 0)
				size = 0;
			setCgiEnv("CONTENT_LENGTH", numToString(size));
		}
		switch (route.type)
		{
		case RR_GET:
			setCgiEnv("REQUEST_METHOD", "GET");
		case RR_CGI_POST:
			setCgiEnv("REQUEST_METHOD", "POST");
		case RR_HEAD:
			setCgiEnv("REQUEST_METHOD", "HEAD");
		default:;
		}

		std::string contType = mClient->mParser.getHeaders()["content-type"];

		if (contType.empty() == false)
		{
			setCgiEnv("CONTENT_TYPE", contType);
		}

		std::string ipAddr = mClient->mListener->getIp();

		if (ipAddr.empty() == false)
		{
			setCgiEnv("REMOTE_ADDR", ipAddr);
		}

		std::string serverName = mClient->mConfig->serverName;

		if (serverName.empty() == false)
		{
			setCgiEnv("SERVER_NAME", serverName);
		}

		if (route.filePath.empty() == false)
		{
			setCgiEnv("SCRIPT_NAME", route.filePath);
		}

		setCgiEnv("SERVER_PORT", numToString(mClient->mListener->getPort()));
		setCgiEnv("GATEWAY_INTERFACE", "CGI/1.1");
		setCgiEnv("SERVER_PROTOCOL", "HTTP/1.1");

		mEnvp.push_back(NULL);
		char *const *envp = &mEnvp[0];

		char *argv[3];
		argv[0] = (char *) route.filePath.c_str();
		argv[1] = (char *) route.filePath.c_str();
		argv[2] = NULL;
		LOG_INFO("starting execve");
		execve(argv[0], argv, envp);
		std::string str = std::string(strerror(errno));
		LOG_ERROR("execve failed: " + std::string(strerror(errno)));
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

		setNonBlockingFlag(mOutFd[0]);
		setNonBlockingFlag(mInFd[1]);

		close(this->mOutFd[1]);
		close(this->mInFd[0]);
	}
	return (true);
}

void CgiHandler::handleEvents(struct pollfd &pollStruct)
{
	if (pollStruct.fd == mOutFd[0])
	{
		char buffer[BUFFER_SIZE];
		memset(buffer, 0, BUFFER_SIZE);

		ssize_t len = read(mOutFd[0], buffer, BUFFER_SIZE);

		if (len <= 0)
		{
			mClient->setCgiReady(true);
			setCgiResponse();
		}
		else
		{
			mCgiBody.append(buffer, len);
			mBodySize += len;
		}
		return;
	}

	if (pollStruct.fd == mInFd[1] && (pollStruct.revents & POLLOUT))
	{
		const std::string &reqBodyFile = mClient->getRequestBody();
		std::ifstream	   inf(reqBodyFile.c_str());
		std::stringstream  buffer;
		buffer << inf.rdbuf();

		const std::string &reqBody = buffer.str();
		while (!reqBody.empty())
		{
			if (waitpid(-1, NULL, WNOHANG) > 0)
				break;
			ssize_t written = write(mInFd[1], reqBody.data(), reqBody.size());
			if (written > 0)
				mInBytesWritten += written;
			else
			{
				// fatal error
				pollStruct.events &= ~POLLOUT;
				break;
			}
		}
		if (mInBytesWritten >= reqBody.size())
		{
			pollStruct.events &= ~POLLOUT;
		}
		return;
	}
}

void CgiHandler::setCgiResponse()
{
	std::ostringstream finalResponse;
	std::string		  &clientResponse = mClient->getResponse();

	size_t headerEnd = mCgiBody.find("\r\n\r\n");
	size_t sepLen = 4;
	if (headerEnd == std::string::npos)
	{
		headerEnd = mCgiBody.find("\n\n");
		if (headerEnd == std::string::npos)
			sepLen = 0;
		else
			sepLen = 2;
	}

	size_t		parsePos = 0;
	int			status = 200;
	bool		hasStatus = false;
	bool		hasContentLength = false;
	bool		hasContentType = false;
	std::string statusStr;
	std::string contentLengthStr;
	std::string contentTypeStr;

	std::string headers = mCgiBody.substr(0, headerEnd);
	std::string body;
	if (headerEnd == std::string::npos)
		body = mCgiBody;
	else
		body = mCgiBody.substr(headerEnd + sepLen);

	std::istringstream		 headerStream(headers);
	std::string				 line;
	std::vector<std::string> outHeaders;
	while (std::getline(headerStream, line))
	{
		if (line.empty())
			continue;
		size_t pos = line.find(':');
		if (pos == std::string::npos)
			continue;
		std::string name = line.substr(0, pos);
		std::string value = line.substr(pos + 1);
		for (std::string::iterator it = name.begin(); it != name.end(); it++)
			*it = std::tolower(static_cast<unsigned char>(*it));

		if (name == "content-length")
		{
			hasContentLength = true;
			outHeaders.push_back("Content-Length: " + value);
		}
		else if (name == "content-type")
		{
			hasContentType = true;
			outHeaders.push_back("Content-Type: " + value);
		}
		else if (name == "status")
		{
			hasStatus = true;
			outHeaders.push_back("Status: " + value);
		}
		else
		{
			outHeaders.push_back(name + ": " + value);
		}

		if (!hasContentType)
			outHeaders.insert(outHeaders.begin(), "ContentType: text/html");
		if (!hasContentLength)
			outHeaders.push_back("Content-Length: " + numToString(body.size()));
	}

	// map status to reason phrase (small set)
	std::string reason;
	switch (status)
	{
	case 200:
		reason = "OK";
		break;
	case 201:
		reason = "Created";
		break;
	case 302:
		reason = "Found";
		break;
	case 400:
		reason = "Bad Request";
		break;
	case 404:
		reason = "Not Found";
		break;
	case 500:
	default:
		reason = "Internal Server Error";
		break;
	}
	finalResponse << "HTTP/1.1 " << status << " " << reason << "\r\n";
	for (size_t i = 0; i < outHeaders.size(); i++)
		finalResponse << outHeaders[i] << "\r\n";
	finalResponse << "\r\n";
	finalResponse << body;
	clientResponse = finalResponse.str();
	LOG_DEBUG(finalResponse.str());
	mKeepAlive = false;

	// cleanup
	std::remove(mClient->getRequestBody().c_str());
}

/*
** --------------------------------- ACCESSOR ---------------------------------
*/

// Compulsory function to implement abstract class
int CgiHandler::getFd() const
{
	return (this->mOutFd[0]);
}

int CgiHandler::getOutFd() const
{
	return (this->mOutFd[0]);
}

int CgiHandler::getInFd() const
{
	return (this->mInFd[1]);
}

bool CgiHandler::getKeepAlive() const
{
	return (mKeepAlive);
}

void CgiHandler::terminateCgi()
{
	mKeepAlive = false;
}

/* ************************************************************************** */
