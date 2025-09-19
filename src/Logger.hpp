#ifndef LOGGER_HPP
# define LOGGER_HPP

#include <fstream>
# include <iostream>
# include <string>

enum LogLevel
{
	DEBUG,
	INFO,
	WARNING,
	ERROR,
	FATAL,
};

class Logger
{

	public:

		Logger(const std::string &logFileName);
		Logger( Logger const & src );
		~Logger();

		Logger &		operator=( Logger const & rhs );

		void log(LogLevel level, const std::string &errorMsg);

	private:
		std::ofstream logFile;

};

std::ostream &			operator<<( std::ostream & o, Logger const & i );

#endif /* ********************************************************** LOGGER_H */