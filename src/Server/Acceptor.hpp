#pragma once

#include <iostream>
#include <netinet/in.h>
#include <vector>

class Acceptor
{
public:
	Acceptor();
	~Acceptor();

	bool					initServerPorts(std::vector<int> ports);
	const std::vector<int> &listeners() const;
	void					closeAll();
	void					newConnection(int listenFd);

private:
	std::vector<int> mListeners;

	bool bindPort(sockaddr_in socketStruct);
};
