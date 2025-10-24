#ifndef LOGGER_HPP
#define LOGGER_HPP

# include <fstream>
# include <string>
# include <sstream>

/** \enum LogLevel
    Enum that defines severity of log message.
*/
enum LogLevel
{
	DEBUG,
	NOTICE,
	INFO,
	WARNING,
	ERROR,
	FATAL,
};

/**
    \class Logger
    Logging class created using Singleton design pattern.
*/
class Logger
{
public:
	/** getInstance method

	    getInstance constructs a singleton object on the first run and places
	    it in static. On subsequent runs, it returns the existing object as a
	    pointer.
	*/
	static Logger *getInstance();

	/** log method

	    the log() method is used to log output to console and the log file.
		\param level Enum which states the severity of the message
		\param msg String to output as log message
	*/
	void log(LogLevel level, const std::string &msg);

	/** logFile method

	    the logFile() method only outputs to the log file.
	    this can be used to log things like server requests, which can clutter
	    the console.

		\param level Enum which states the severity of the message
		\param msg String to output as log message
	*/
	void logFile(LogLevel level, const std::string &msg);
	~Logger();

protected:
	Logger(); /**< Private constructor */

private:
	std::ofstream mFile;    /**< log file stream object */
	static Logger *mLogger; /**< Static pointer to singleton instance */
};

/** \example
    This is an example of how to use the Logger class

    \code{.cpp}
	#include "Logger.hpp"

	// creates a pointer to the single instance of logger
    Logger *log = Logger::getInstance();

	// use the -> operator to access the log methods
	// first param is the log level, second is the message
    log->log(INFO, "server now listening for client connections");
    log->logFile(NOTICE, "insert client server message here");
    \endcode
*/

/**
    \fn numToString
    Template function which converts a number to string, since std::to_string
    is not available in cpp98
*/
template<typename T> std::string numToString(T n)
{
	std::ostringstream buf;
	buf << n;
	return buf.str();
}

#endif /* ********************************************************** LOGGER_H  \
        */
