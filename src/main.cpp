#include "WebServer.hpp"
#include "configParser.hpp"

int main()
{
	configParser parser = configParser("default.conf");
	// const ServerConfig &srv = parser.servers[0];

	WebServer webServer = WebServer("127.0.0.1", parser.servers);
	webServer.startListen();
	return 0;
}
