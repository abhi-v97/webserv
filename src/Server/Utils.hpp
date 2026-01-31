#pragma once
#include <iostream>
#include <sys/socket.h>

bool		setNonBlockingFlag(int socketFd);
std::string generateId();
