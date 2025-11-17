#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include <arpa/inet.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <netinet/in.h>
#include <poll.h> // used for poll()
#include <string>
#include <sys/poll.h>
#include <sys/socket.h>
#include <vector>

#include "CgiHandler.hpp"
#include "Logger.hpp"
#include "RequestParser.hpp"
#include "ResponseBuilder.hpp"

// TODO: global vars for now, store them later in a struct or macro
// struct is probably better if we want to be able to modify these through the
// config file
static const size_t MAX_REQUEST_LINE =
	static_cast<const size_t>(8 * 1024); // 8kb
static const size_t MAX_HEADER_FIELD =
	static_cast<const size_t>(4 * 1024);    // 4kb
static const size_t MAX_HEADERS_TOTAL = 16; // max num of header fields.
static const size_t MAX_BODY_IN_MEM = static_cast<const size_t>(
	16 * 1024); // body size to store as a buffer, 16kb for test
static const size_t MAX_BODY_TOTAL =
	static_cast<const size_t>(1 * 1024 * 1024); // 1mb max size for body
#include "configParser.hpp"

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

	RequestParser parser;        /**< Request parser object */
	ResponseBuilder responseObj; /**< Response builder object */
	CgiHandler cgiObj;           /**< CGI handler object */
};

/** \class WebServer
    Creates a server with a given IP address and port and handles client
    requests.
*/
class WebServer
{
public:
	WebServer(std::string ipAddress, int port);
	WebServer(std::string ipAddress, const std::vector<ServerConfig> &srv);
	~WebServer();

	int bindPort(sockaddr_in socketStruct);
	void closeServer() const;

	void startListen();
	void acceptConnection(int socket);

	void parseRequest(int clientNum);

	static std::string defaultResponse();
	bool sendResponse(int clientNum);
	void generateResponse(int clientNum);

private:
	std::string mIpAddress; /**< Server IP Address, stored as std::string*/
	// int mPort;              /**< Server listening port */
	// int mListenSocket; /**< Server socket for incoming requests */
	// TODO: do you need to create and store a struct for listening sockets?
	// Atm they are not being used outside of bindPort()
	std::vector<int> mSocketVector;
	std::vector<sockaddr_in>
		mSocketAddressStruct; /**< Vector of sockaddr_in structs used to bind
	                            server port and listening socket */
	// struct sockaddr_in mSocketAddress; /**< struct needed by bind() */
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
