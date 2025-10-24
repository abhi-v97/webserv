#include "configParser.hpp"

configParser::configParser(const std::string &filename) : input(readFile(filename)), lexer(input) {
	advance();
	parseConfig();
}

configParser   &configParser::operator=(const configParser &other) {
	if (this != &other)
	{
		input = other.input;
		lexer = other.lexer;
		current = other.current;
		servers = other.servers;
	}
	return *this;
}

configParser::configParser(const configParser &other) {
	*this = other;
}

configParser::configParser() {
	
}

configParser::~configParser() {
	
}

std::string	configParser::readFile(const std::string &filename)
{
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        throw std::runtime_error("Could not open config file: " + filename);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void	configParser::advance()
{
		current = lexer.getNextToken();
}

void    configParser::expect(configTokenType type)
{
	if (current.type != type)
		throw std::runtime_error("Unexpected token: " + current.value);
	advance();
}

ServerConfig	configParser::parseServerBlock()
{
	ServerConfig	cfg;
	int				port;

	cfg.clientMaxBodySize = 0;
	if (current.type != WORD || current.value != "server")
		throw std::runtime_error("Expected 'server'");
	advance();
	expect(LBRACE);
	while (current.type != RBRACE && current.type != END)
	{
		if (current.type == WORD && current.value == "listen")
		{
			advance();
			std::stringstream ss(current.value);
			ss >> port;
			if (current.type != WORD)
				throw std::runtime_error("Expected port number");
			cfg.listenPorts.push_back(port);
			advance();
			expect(SEMICOLON);
		}
		else if (current.type == WORD && current.value == "root")
		{
			advance();
			if (current.type != WORD)
				throw std::runtime_error("Expected root path");
			cfg.root = current.value;
			advance();
			expect(SEMICOLON);
		}
		else if (current.value == "location")
		{
			LocationConfig loc = parseLocationBlock();
			cfg.locations.push_back(loc);
		}
		else if (current.type == WORD && current.value == "error_page")
		{
			advance();
			if (current.type != WORD)
				throw std::runtime_error("Expected error code");
			int	code;
			code = 0;
			std::stringstream ss(current.value);
			ss >> code;
			advance();
			if (current.type != WORD)
				throw std::runtime_error("Expected error page path");
			std::string path = current.value;
			advance();
			expect(SEMICOLON);
			cfg.errorPages[code] = path;
		}
		else if (current.type == WORD && current.value == "client_max_body_size")
		{
			advance();
			if (current.type != WORD)
				throw std::runtime_error("Expected max body size value");
			std::stringstream ss(current.value);
			ss >> cfg.clientMaxBodySize;
			advance();
			expect(SEMICOLON);
		}
		else
			throw std::runtime_error("Unknown directive: " + current.value);
	}
	expect(RBRACE);
	return (cfg);
}

void	configParser::parseConfig()
{
	while (current.type != END)
	{
		ServerConfig server = parseServerBlock();
		this->servers.push_back(server);
	}
}

LocationConfig	configParser::parseLocationBlock()
{
	LocationConfig	loc;

	loc.redirectErr = 0;
	if (current.type != WORD || current.value != "location")
		throw std::runtime_error("Expected 'location'");
	advance();
	if (current.type != WORD)
		throw std::runtime_error("Expected location path");
	loc.path = current.value;
	advance();
	expect(LBRACE);
	while (current.type != RBRACE && current.type != END)
	{
		if (current.value == "root")
		{
			advance();
			loc.root = current.value;
			advance();
			expect(SEMICOLON);
		}
		else if (current.value == "autoindex")
		{
			advance();
			if (current.value == "on")
				loc.autoindex = true;
			else if (current.value == "off")
				loc.autoindex = false;
			else
				throw std::runtime_error("Expected 'on' or 'off' after autoindex");
			advance();
			expect(SEMICOLON);
		}
		else if (current.value == "allow_methods")
		{
			advance();
			while (current.type == WORD)
			{
				loc.methods.push_back(current.value);
				advance();
			}
			expect(SEMICOLON);
		}
		else if (current.value == "index")
		{
			advance();
			loc.index = current.value;
			advance();
			expect(SEMICOLON);
		}
		else if (current.value == "upload_dir")
		{
			advance();
			loc.uploadDir = current.value;
			advance();
			expect(SEMICOLON);
		}
		else if (current.value == "return")
		{
			advance();
			if (current.type != WORD)
				throw std::runtime_error("Expected error code");
			int	code = 0;
			std::stringstream ss(current.value);
			ss >> code;
			advance();
			if (current.type != WORD)
				throw std::runtime_error("Expected return page path");
			std::string returnPage = current.value;
			advance();
			expect(SEMICOLON);
			loc.redirect = returnPage;
			loc.redirectErr = code;
		}
		else
			throw std::runtime_error("Unknown directive in location: " + current.value);
	}
	expect(RBRACE);
	return (loc);
}
