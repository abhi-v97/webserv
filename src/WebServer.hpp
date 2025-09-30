#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <poll.h> // used for poll()
#include <string>
#include <sys/poll.h>
#include <sys/socket.h>
#include <vector>

struct ClientState
{
	int fd;
	std::string response;
	std::string request;
	ssize_t bytesRead;
	ssize_t bytesSent;
};

class WebServer
{
public:
	WebServer();
	WebServer(const WebServer &src);
	WebServer(std::string ipAddress, int port);
	~WebServer();

	WebServer &operator=(const WebServer &rhs);

	int startServer();
	void closeServer() const;

	void startListen();
	void acceptConnection();

	void parseRequest(int clientNum);

	static std::string defaultResponse();
	bool sendResponse(int clientFd);
	void generateResponse(int clientFd);

private:
	std::string mIpAddress;
	int mPort;
	int mListenSocket;
	struct sockaddr_in mSocketAddress;
	unsigned int mSocketAdddressLen;
	std::map<int, ClientState> mClients;
	std::vector<pollfd> mPollFdVector;
	int mSignal;
};

std::ostream &operator<<(std::ostream &outf, const WebServer &src);

bool setNonBlockingFlag(int socketFd);
void signalHandler(int sig);

#endif /* ******************************************************* WEBSERVER_H  \
        */
