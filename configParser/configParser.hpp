#ifndef CONFIGPARSER_HPP
# define CONFIGPARSER_HPP

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <cctype>
#include <vector>
#include <map>
#include <iostream>
#include "configLexer.hpp"

struct LocationConfig {
	std::vector<std::string>	methods;
	std::string				 	redirect;
	int							redirectErr;
	std::string 				root;
	bool						autoindex;
	std::string 				index;
	std::string 				uploadDir;
	std::string 				path;
};

struct ServerConfig {
	std::string					root;
	std::vector<int> 			listenPorts;
	std::map<int, std::string>	errorPages;
	size_t						clientMaxBodySize;
	std::vector<LocationConfig>	locations;
};

class configParser {
	private:
		std::string			input;
		configLexer			lexer;
		configToken			current;
		void				advance();
		void				expect(configTokenType type);
		LocationConfig		parseLocationBlock();
		ServerConfig		parseServerBlock();
		void				parseConfig();
		std::string			readFile(const std::string &filename);

	public:
		configParser(const std::string &filename);
		configParser &operator=(const configParser &other);
		configParser(const configParser &other);
		configParser();
		~configParser();

		std::vector<ServerConfig>	servers;
}; 

#endif