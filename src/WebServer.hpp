#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include <arpa/inet.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/socket.h>

class WebServer
{
public:
	WebServer();
	WebServer(const WebServer &src);
	WebServer(std::string ipAddress, int port);
	~WebServer();

	WebServer &operator=(const WebServer &rhs);

	int startServer();
	void closeServer();

	void startListen();
	void acceptConnection(struct pollfd *pollFds);

	std::string defaultResponse();
	void sendResponse();

private:
	std::string m_ipAddress;
	int m_port;
	int m_socket;
	int m_newSocket;
	int	m_clientCount;
	long m_incomingMessage;
	struct sockaddr_in m_socketAddress;
	unsigned int m_socketAdddress_len;
	std::string m_serverResponse;
};

std::ostream &operator<<(std::ostream &o, const WebServer &i);

	
// testing, function will be used to change socket_fd to nonblocking
bool	setNonBlockingFlag(int socketFd);


#endif /* ******************************************************* WEBSERVER_H  \
        */
