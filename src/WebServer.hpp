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

#include "Logger.hpp"

/** \struct ClientState
    Struct used to store client information
*/
struct ClientState
{
	int fd;               /**< socket fd used to listen to client requests */
	std::string response; /**< placeholder; stores client response in its
	                        entirety */
	std::string request;  /**< placeholder; stores client request */
	ssize_t bytesRead;    /**< number of bytes read from a client request, which
	                        allows requests to be read in chunks */
	ssize_t bytesSent;    /**< bytes sent of the response, to allow the response
	                        being sent in chunks */
	std::string clientIp; /**< Ip address of client */
};

/** \class WebServer
    Creates a server with a given IP address and port and handles client
    requests.
*/
class WebServer
{
public:
	WebServer(std::string ipAddress, int port);
	~WebServer();

	int startServer();
	void closeServer() const;

	void startListen();
	void acceptConnection();

	void parseRequest(int clientNum);

	static std::string defaultResponse();
	bool sendResponse(int clientFd);
	void generateResponse(int clientFd);

private:
	std::string mIpAddress; /**< Server IP Address, stored as std::string*/
	int mPort;              /**< Server listening port */
	int mListenSocket;      /**< Server socket for incoming requests */
	struct sockaddr_in mSocketAddress; /**< struct needed by poll() */
	unsigned int mSocketAdddressLen;   /**< size of sockaddr_in struct */
	std::map<int, ClientState>
		mClients;                      /**< map container to hold client info */
	std::vector<pollfd> mPollFdVector; /**< vector container to hold socket
	                                     info for each connection*/
	Logger *mLog;
};

bool setNonBlockingFlag(int socketFd);
void signalHandler(int sig);

#endif /* ******************************************************* WEBSERVER_H  \
        */
