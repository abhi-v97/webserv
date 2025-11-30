#pragma once

#include <string>
#include <sys/socket.h>

bool	setNonBlockingFlag(int socketFd);
int		make_listen_socket(const struct sockaddr_in &addr, int backlog /*= LISTEN_REQUESTS*/);
int		accept_nonblocking(int listen_fd, struct sockaddr_in *out_addr, socklen_t *out_len);
ssize_t drain_recv(int fd, std::string &out); // read until EAGAIN and append
ssize_t write_remaining(int fd, const std::string &buf, ssize_t &bytesSent); // send pa
