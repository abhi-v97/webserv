
#include <ctime>
#include <iomanip>
#include "Logger.hpp"

/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

Logger::Logger(const std::string &logFileName)
{
	logFile.open(logFileName.c_str(), std::ios::app);
}

Logger::Logger( const Logger & src )
{
}


/*
** -------------------------------- DESTRUCTOR --------------------------------
*/

Logger::~Logger()
{
}


/*
** --------------------------------- OVERLOAD ---------------------------------
*/

Logger &				Logger::operator=( Logger const & rhs )
{
	//if ( this != &rhs )
	//{
		//this->_value = rhs.getValue();
	//}
	return *this;
}

std::ostream &			operator<<( std::ostream & o, Logger const & i )
{
	//o << "Value = " << i.getValue();
	return o;
}


/*
** --------------------------------- METHODS ----------------------------------
*/

std::string levelToString(LogLevel level)
{
	switch (level) {
	case DEBUG:
		return "DEBUG";
	case INFO:
		return "INFO";
	case WARNING:
		return "WARNING";
	case ERROR:
		return "ERROR";
	case FATAL:
		return "FATAL";
	}
	return ("UNKNOWN");
}

std::string getTimestamp(void) {
	std::time_t timeNow;
	struct tm *timeStruct;
	char buffer[50];
	
	std::time(&timeNow);
	timeStruct = std::localtime(&timeNow);
	std::strftime(buffer, 50, "%Y-%m-%d %H:%M:%S ", timeStruct);
	return (buffer);
}

void Logger::log(LogLevel level, const std::string &errorMsg)
{
	logFile << " [ " << levelToString(level) << " ] ";
	logFile << getTimestamp() << " : ";
	logFile << errorMsg << std::endl;
}

/*
** --------------------------------- ACCESSOR ---------------------------------
*/


/* ************************************************************************** */