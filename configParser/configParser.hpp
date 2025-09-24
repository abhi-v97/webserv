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
	std::string 				path;
	std::string 				root;
	bool						autoindex;
	std::vector<std::string>	methods;
	std::string 				index;
	std::string 				uploadDir;
	std::string 				redirect;
};

struct ServerConfig {
	std::vector<int> 			listenPorts;
	std::string					root;
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