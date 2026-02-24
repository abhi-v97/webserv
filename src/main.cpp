#include "Dispatcher.hpp"
#include "Logger.hpp"
#include "MimeTypes.hpp"
#include "configParser.hpp"

int main(int argc, char **argv)
{
	std::string configFile;
	
	srand(time(0));

	if (argc < 2)
		configFile = "default.conf";
	else
		configFile = std::string(argv[1]);
	configParser			  parser(configFile);
	Dispatcher				  dispatch;
	std::vector<ServerConfig> srv;

	srv = parser.servers;
	for (std::vector<ServerConfig>::iterator serverIter = srv.begin(); serverIter != srv.end(); serverIter++)
	{
		std::vector<int> &ports = (*serverIter).listenPorts;
		for (std::vector<int>::iterator portIter = ports.begin(); portIter != ports.end(); portIter++)
			dispatch.createListener(*portIter, *serverIter);
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
