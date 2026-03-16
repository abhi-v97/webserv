#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "configLexer.hpp"

struct CgiConfig
{
	std::string						   extension;
	std::string						   pass;
	std::map<std::string, std::string> params;
	std::vector<std::string>		   methods;
};

struct LocationConfig
{
	std::vector<std::string> methods;
	std::string				 redirect;
	std::string				 alias;
	int						 redirectErr;
	std::string				 root;
	bool					 autoindex;
	std::string				 index;
	std::string				 uploadDir;
	std::string				 path;
	std::vector<CgiConfig>	 cgis;
	bool					 cgiEnabled;
};

struct ListenConfig
{
	std::string IP;
	int			port;
};

struct ServerConfig
{
	std::string					serverName;
	std::string					root;
	std::vector<ListenConfig>	listenConfigs;
	std::map<int, std::string>	errorPages;
	size_t						clientMaxBodySize;
	std::vector<LocationConfig> locations;
};

class configParser
{
private:
	static size_t const					DEFAULT_CLIENT_MAX_BODY_SIZE;
	static const char					DEFAULT_IP_ADDRESS[];
	std::string							input;
	configLexer							lexer;
	configToken							current;
	void								advance();
	void								expect(configTokenType type);
	LocationConfig						parseLocationBlock();
	ServerConfig						parseServerBlock();
	void								parseErrorPage(ServerConfig &cfg);
	void								parseClientMaxBodySize(ServerConfig &cfg);
	void								parseListen(ServerConfig &cfg, int &port);
	std::pair<std::string, std::string> splitAddressPort(const std::string &token);
	void								validatePortString(const std::string &portStr);
	void								validateAddress(const std::string &addr);
	void		addListen(ServerConfig &cfg, const std::string &addr, int port);
	void		parseRootDirective(ServerConfig &cfg);
	void		parseServerName(ServerConfig &cfg);
	void		parseConfig();
	void		parseAutoindex(LocationConfig &loc);
	void		parseReturn(LocationConfig &loc);
	void		parseAllowMethods(LocationConfig &loc);
	void		parseCgiBlock(LocationConfig &loc);
	void		parseCgiExtension(CgiConfig &cfg);
	void		parseCgiPass(CgiConfig &cfg);
	void		parseCgiParam(CgiConfig &cfg);
	void		parseCgiMethod(CgiConfig &cfg);
	std::string readFile(const std::string &filename);

public:
	configParser(const std::string &filename);
	configParser &operator=(const configParser &other);
	configParser(const configParser &other);
	configParser();
	~configParser();

	std::vector<ServerConfig> servers;
	void		outputServerLogs(const ServerConfig &cfg) const;
};

#endif
