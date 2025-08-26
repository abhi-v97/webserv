#ifndef WEBSERVER_HPP
# define WEBSERVER_HPP

# include <iostream>
# include <string>
# include <sys/socket.h>
# include <arpa/inet.h>
#include <stdlib.h>

class WebServer
{
	public:

		WebServer();
		WebServer( WebServer const & src );
		WebServer(std::string ipAddress, int port);
		~WebServer();

		WebServer &		operator=( WebServer const & rhs );

		int	startServer();
		void closeServer();
		
		void startListen();
		void acceptConnection(int socket);

		std::string getResponse();
		void sendResponse();

	private:
		std::string			m_ipAddress;
		int					m_port;
		int					m_socket;
		int					m_newSocket;
		long				m_incomingMessage;
		struct sockaddr_in	m_socketAddress;
		unsigned int		m_socketAdddress_len;
		std::string			m_serverMessage;

};

std::ostream &			operator<<( std::ostream & o, WebServer const & i );

#endif /* ******************************************************* WEBSERVER_H */