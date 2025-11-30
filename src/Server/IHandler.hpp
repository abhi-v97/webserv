#pragma once

#include <poll.h>

class IHandler
{
public:
	IHandler();
	virtual ~IHandler();

	virtual void handleEvents(struct pollfd &pollStruct) = 0;
	virtual int	 getFd() const = 0;
};
