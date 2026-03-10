#pragma once

#include <poll.h>

/**
	\class IHandler

	Abstract class to implement a handler that can be polled and managed by Dispatcher.
*/
class IHandler
{
public:
	IHandler();
	virtual ~IHandler();

	virtual void handleEvents(struct pollfd &pollStruct) = 0;
	virtual int	 getFd() const = 0;
	virtual bool getKeepAlive() const = 0;

	bool mKeepAlive;
};
