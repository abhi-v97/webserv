#include "Dispatcher.hpp"
#include "Logger.hpp"
#include "MimeTypes.hpp"
#include "configParser.hpp"

int main(int argc, char **argv)
{
	std::string configFile;

	if (argc < 2)
		configFile = "default.conf";
	else
		configFile = std::string(argv[1]);
	configParser			  parser(configFile);
	Dispatcher				  dispatch;
	std::vector<ServerConfig> srv;

	srv = parser.servers;
	for (std::vector<ServerConfig>::iterator sit = srv.begin(); sit != srv.end(); ++sit)
	{
		for (std::vector<ListenConfig>::iterator lit = sit->listenConfigs.begin(); lit != sit->listenConfigs.end(); ++lit)
		{
			dispatch.createListener(lit->IP, lit->port);
			// dispatch.createListener(lit->IP, lit->port);
		}
	}

	if (dispatch.setListeners() == false)
		return (1);

	// load supported types
	MimeTypes::getInstance()->loadFromFile("mime.types");

	dispatch.loop();

	Logger::deleteInstance();
	MimeTypes::deleteInstance();
	return (0);
}
