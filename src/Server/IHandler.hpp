#pragma once

class IHandler
{
public:
	IHandler();
	virtual ~IHandler();

	virtual void handleEvents(short revents) = 0;
	virtual int	 getFd() const = 0;
};
