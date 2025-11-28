#include <ctime>
#include <iostream>
#include <ostream>
#include <sstream>

#include "Logger.hpp"

#define BUF_SIZE 50


// static member init
Logger *Logger::mLogger = NULL;

/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

Logger::Logger()
{
	std::time_t timeNow;
	struct tm  *timeStruct;
	char		buffer[BUF_SIZE];

	std::time(&timeNow);
	timeStruct = std::localtime(&timeNow);
	std::strftime(buffer, BUF_SIZE, "%Y-%m-%d_%H:%M:%S.log", timeStruct);
	Logger::mFile.open(buffer, std::ios::app);
}

/*
** -------------------------------- DESTRUCTOR --------------------------------
*/

void Logger::deleteLogger()
{
	if (Logger::mLogger)
	{
		delete Logger::mLogger;
		Logger::mLogger = NULL;
	}
}

Logger::~Logger()
{
	Logger::mFile.close();
}

/*
** --------------------------------- METHODS ----------------------------------
*/

Logger *Logger::getInstance()
{
	if (mLogger == NULL)
	{
		mLogger = new Logger();
	}
	return mLogger;
}

/**
	Helper function which converts enum to string.
*/
std::string levelToString(LogLevel level)
{
	switch (level)
	{
	case DEBUG:
		return "debug";
	case NOTICE:
		return "notice";
	case INFO:
		return "info";
	case WARNING:
		return "warning";
	case ERROR:
		return "error";
	case FATAL:
		return "fatal";
	}
	return "unknown";
}

/**
	Helper function which gets the current timestamp
*/
std::string getTimestamp(void)
{
	std::time_t timeNow;
	struct tm  *timeStruct;
	char		buffer[BUF_SIZE];

	std::time(&timeNow);
	timeStruct = std::localtime(&timeNow);
	std::strftime(buffer, BUF_SIZE, "%Y-%m-%d %H:%M:%S ", timeStruct);
	return buffer;
}

/**
	\details
	Ostringstream creates a std::string buffer where the log message is
	first written. Then its forwarded to cout and logFile.
*/
void Logger::log(LogLevel level, const std::string &msg)
{
	static std::ostringstream buf;
	buf.str("");
	buf.clear();

	buf << getTimestamp();
	buf << "[" << levelToString(level) << "] : ";
	buf << msg << '\n';

	std::string line = buf.str();
	mFile << line;
	mFile.flush();
	std::cout << line;
	std::cout.flush();
}

/*
** --------------------------------- ACCESSOR ---------------------------------
*/

/* ************************************************************************** */
