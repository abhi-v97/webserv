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

/**
	\brief constructor for the Logger object. 
	opens two log file on startup:
	- a file named "access.log" for access logs NOTICE, INFO and DEBUG messages (request traffic)
	- a file named "error.log" for error logs WARNING, ERROR and FATAL messages (problems)
*/
Logger::Logger()
{
	mAccessFile.open("access.log", std::ios::app);
	mErrorFile.open("error.log", std::ios::app);
	// std::time_t timeNow;
	// struct tm  *timeStruct;
	// char		buffer[BUF_SIZE];

	// std::time(&timeNow);
	// timeStruct = std::localtime(&timeNow);
	// std::strftime(buffer, BUF_SIZE, "%d/%m/%Y_%H:%M:%S.log", timeStruct);
	// Logger::mFile.open(buffer, std::ios::app);
}

/*
** -------------------------------- DESTRUCTOR --------------------------------
*/

/**
	\brief Destructor, deletes the singleton instance if found.
*/
void Logger::deleteInstance()
{
	if (Logger::mLogger)
	{
		delete Logger::mLogger;
		Logger::mLogger = NULL;
	}
}

Logger::~Logger()
{
	// Logger::mFile.close();
	mAccessFile.close();
	mErrorFile.close();
}

/*
** --------------------------------- METHODS ----------------------------------
*/

/**
	\brief Getter for the Logger singleton instance
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
	\brief Helper function which converts enum to string.
*/
std::string levelToString(LogLevel level)
{
	switch (level)
	{
	case DEBUG:
		return "debug";
	case INFO:
		return "info";
	case NOTICE:
		return "notice";
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
	\brief Helper function which gets the current timestamp
*/
std::string getTimestamp(void)
{
	std::time_t timeNow;
	struct tm  *timeStruct;
	char		buffer[BUF_SIZE];

	std::time(&timeNow);
	timeStruct = std::localtime(&timeNow);
	std::strftime(buffer, BUF_SIZE, "%d/%m/%Y %H:%M:%S ", timeStruct);
	return buffer;
}

/**
	\brief
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
	//route log message to the correct file based on its level
	if (level >= WARNING) {
		mErrorFile << line;
		mErrorFile.flush();
	}
	else {
		mAccessFile << line;
		mAccessFile.flush();
	}
	// mFile << line;
	// mFile.flush();
	std::cout << line;
	std::cout.flush();
}

/*
** --------------------------------- ACCESSOR ---------------------------------
*/

/* ************************************************************************** */
