#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <fstream>
#include <sstream>
#include <string>

// TODO: put macros in a separate file?

/** \example
	This is an example of how to use the Logger class

	\code{.cpp}
	#include "Logger.hpp"

	// compile with "make info"
	LOG_DEBUG("debug text");   // compiled out
	LOG_INFO("info text");     // compiled in
	\endcode
*/

/**
	## Usage Guidelines
	- DEBUG: use to log function calls or code path analysis
	- INFO & NOTICE: informational messages about server state
	- WARNING: warnings that don't halt execution
	- ERROR: common errors that cause a halt in processing eg: closing client connection
	- FATAL: anything that causes web server to shut down
*/

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_NOTICE 2
#define LOG_LEVEL_WARNING 3
#define LOG_LEVEL_ERROR 4
#define LOG_LEVEL_FATAL 5

// set default compile level
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG(msg) Logger::getInstance()->log(DEBUG, msg)
#else
#define LOG_DEBUG(msg) ((void) 0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_INFO
#define LOG_INFO(msg) Logger::getInstance()->log(INFO, msg)
#else
#define LOG_INFO(msg) ((void) 0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_NOTICE
#define LOG_NOTICE(msg) Logger::getInstance()->log(NOTICE, msg)
#else
#define LOG_NOTICE(msg) ((void) 0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_WARNING
#define LOG_WARNING(msg) Logger::getInstance()->log(WARNING, msg)
#else
#define LOG_WARNING(msg) ((void) 0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define LOG_ERROR(msg) Logger::getInstance()->log(ERROR, msg)
#else
#define LOG_ERROR(msg) ((void) 0)
#endif

#if LOG_LEVEL <= LOG_LEVEL_FATAL
#define LOG_FATAL(msg) Logger::getInstance()->log(FATAL, msg)
#else
#define LOG_FATAL(msg) ((void) 0)
#endif

/** \enum LogLevel
    Enum that defines severity of log message.
*/
enum LogLevel
{
	DEBUG,
	INFO,
	NOTICE,
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

	static void deleteInstance(); /**< use in main.cpp to free Logger object */
	~Logger();					  /**< deconstructor */

protected:
	Logger(); /**< Private constructor */

private:
	std::ofstream  mFile;	/**< log file stream object */
	static Logger *mLogger; /**< Static pointer to singleton instance */
};

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

#endif /* ********************************************************** LOGGER_H                      \
		*/
