#include "Logger.hpp"
#include "Acceptor.hpp"
#include "Reactor.hpp"
#include "configParser.hpp"

int main(int argc, char **argv)
{
	std::string configFile;

	if (argc < 2)
		configFile = "default.conf";
	else
		configFile = std::string(argv[1]);

	Acceptor				  acceptor;
	configParser			  parser(configFile);
	std::vector<ServerConfig> srv;

	srv = parser.servers;
	for (std::vector<ServerConfig>::iterator it = srv.begin(); it != srv.end(); it++)
	{
		acceptor.init((*it).listenPorts);
	}
	
	Reactor reactor;

	reactor.setListeners(acceptor.listeners());
	reactor.loop();

	Logger::deleteLogger();
	return (0);
}
