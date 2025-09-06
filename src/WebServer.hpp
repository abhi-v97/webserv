#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <poll.h> // used for poll()
#include <string>
#include <sys/socket.h>

// used to configure poll()
// TODO: rejig the struct to be dynamic
#define MAX_CLIENTS 10

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
	
	void parseRequest(int i);

	static std::string defaultResponse();
	bool sendResponse(int clientFd);
	void generateResponse(int clientFd);

private:
	std::string mIpAddress;
	int mPort;
	int mListenSocket;
	int mClientCount;
	struct sockaddr_in mSocketAddress;
	unsigned int mSocketAdddressLen;
	std::map<int, ClientState> mClients;
	struct pollfd mPollFdStruct[MAX_CLIENTS];
};

std::ostream &operator<<(std::ostream &outf, const WebServer &src);

// testing, function will be used to change socket_fd to nonblocking
bool setNonBlockingFlag(int socketFd);

#endif /* ******************************************************* WEBSERVER_H  \
        */
