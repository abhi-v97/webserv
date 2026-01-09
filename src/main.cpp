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
	for (std::vector<ServerConfig>::iterator it = srv.begin(); it != srv.end(); it++)
	{
		std::vector<int> &ports = (*it).listenPorts;
		for (std::vector<int>::iterator it = ports.begin(); it != ports.end(); it++)
			dispatch.createListener(*it);
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
