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
#include <algorithm>
#include "configLexer.hpp"

struct CgiConfig {
	std::string					extension;
	std::string					pass;
	std::map<std::string,std::string> params;
};

struct LocationConfig {
	std::vector<std::string>	methods;
	std::string				 	redirect;
	int							redirectErr;
	std::string 				root;
	bool						autoindex;
	std::string 				index;
	std::string 				uploadDir;
	std::string 				path;
	std::vector<CgiConfig>		cgis;
	bool						cgiEnabled;
};

struct ServerConfig {
	std::string					serverName;
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
		void				parseErrorPage(ServerConfig &cfg);
		void				parseClientMaxBodySize(ServerConfig &cfg);
		void				parseListen(ServerConfig &cfg, int &port);
		void				parseRootDirective(ServerConfig &cfg);
		void				parseServerName(ServerConfig &cfg);
		void				parseConfig();
		void				parseAutoindex(LocationConfig &loc);
		void				parseReturn(LocationConfig &loc);
		void				parseAllowMethods(LocationConfig &loc);
		void				parseCgiBlock(LocationConfig &loc);
		void				parseCgiExtension(CgiConfig &cfg);
		void				parseCgiPass(CgiConfig &cfg);
		void				parseCgiParam(CgiConfig &cfg);
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