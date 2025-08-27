#include "WebServer.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <unistd.h>
/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

WebServer::WebServer() {}

WebServer::WebServer(const WebServer &src) { (void)src; }

WebServer::WebServer(std::string ipAddress, int port)
    : m_ipAddress(ipAddress), m_port(port), m_socket(), m_newSocket(),
      m_incomingMessage(), m_socketAddress(),
      m_socketAdddress_len(sizeof(m_socketAddress)),
      m_serverMessage(getResponse()) {
  m_socketAddress.sin_family = AF_INET;
  m_socketAddress.sin_port = htons(m_port);
  m_socketAddress.sin_addr.s_addr = inet_addr(m_ipAddress.c_str());

  if (startServer() > 0) {
    std::cerr << "cannot connect to socket: " << m_port << ": "
              << std::strerror(errno) << std::endl;
  }
}

/*
** -------------------------------- DESTRUCTOR --------------------------------
*/

WebServer::~WebServer() { closeServer(); }

/*
** --------------------------------- OVERLOAD ---------------------------------
*/

WebServer &WebServer::operator=(WebServer const &rhs) {
  (void)rhs;
  return *this;
}

std::ostream &operator<<(std::ostream &o, WebServer const &i) {
  // o << "Value = " << i.getValue();
  (void)o;
  return o;
}

/*
** --------------------------------- METHODS ----------------------------------
*/

int WebServer::startServer() {
  m_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (m_socket < 0) {
    std::cerr << "socket() failed" << std::strerror(errno) << std::endl;
    return (1);
  }
  if (bind(m_socket, (sockaddr *)&m_socketAddress, m_socketAdddress_len) < 0) {
    std::cerr << "bind() failed" << std::strerror(errno) << std::endl;
    return (1);
  }
  return 0;
}

void WebServer::closeServer() {
  close(m_socket);
  close(m_newSocket);
  exit(0);
}

void WebServer::startListen() {
  if (listen(m_socket, 5) < 0) {
    std::cerr << "listen() failed" << std::strerror(errno) << std::endl;
    return;
  }
  acceptConnection(0);
}

void WebServer::acceptConnection(int socket) {
  socket =
      accept(m_socket, (sockaddr *)&m_socketAddress, &m_socketAdddress_len);
  if (socket < 0) {
    std::cerr << "accept() failed" << std::strerror(errno) << std::endl;
  }
}

std::string WebServer::getResponse() { return ("idk"); }

void WebServer::sendResponse() {
  int bytesSent;
  int totalBytes = 0;

  while (totalBytes < m_serverMessage.size()) {
    bytesSent =
        send(m_newSocket, m_serverMessage.c_str(), m_serverMessage.size(), 0);
    if (bytesSent < 0)
      break;
    totalBytes += bytesSent;
  }
  if (totalBytes != m_serverMessage.size())
    std::cout << "send() failed" << std::endl;
}

/*
** --------------------------------- ACCESSOR ---------------------------------
*/

/* ************************************************************************** */