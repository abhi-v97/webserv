#pragma once

#include <iostream>
#include <netinet/in.h>
#include <vector>

class Acceptor
{
public:
	Acceptor();
	~Acceptor();

	bool					init(std::vector<int> ports);
	const std::vector<int> &listeners() const;
	void					closeAll();

private:
	std::vector<int> mListeners;

	bool bindPort(sockaddr_in socketStruct);
	int createListenSocket(const sockaddr_in &addr);
};
