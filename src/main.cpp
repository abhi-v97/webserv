#include "RequestParser.hpp"
#include "WebServer.hpp"
#include "configParser.hpp"

int main(int argc, char **argv)
{
	std::string configFile;

	if (argc < 2)
	{
		configFile = "default.conf";
	}
	else
	{
		configFile = std::string(argv[1]);
	}
	configParser parser(configFile);
	const ServerConfig &srv = parser.servers[0];

	WebServer webServer = WebServer("127.0.0.1", srv.listenPorts[0]);
	webServer.startListen();

	// RequestParser test = RequestParser();
	//
	// test.parse("test");
	return 0;
}
