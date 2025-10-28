#include "WebServer.hpp"
#include "Logger.hpp"
#include "configParser.hpp"

int main()
{
	configParser parser("default.conf");
	const ServerConfig& srv = parser.servers[0];

	WebServer webServer = WebServer("127.0.0.1", srv.listenPorts[0]);
	webServer.startListen();
	return 0;
}
