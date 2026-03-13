#include <exception>

#include "Dispatcher.hpp"
#include "Logger.hpp"
#include "MimeTypes.hpp"
#include "configParser.hpp"

int main(int argc, char **argv)
{
	std::string				  configFile;
	Dispatcher				  dispatch;
	std::vector<ServerConfig> srv;

	srand(time(0));

	if (argc < 2)
		configFile = "default.conf";
	else
		configFile = std::string(argv[1]);
	try
	{
		configParser parser(configFile);
		srv = parser.servers;
		for (std::vector<ServerConfig>::iterator serverIter = srv.begin(); serverIter != srv.end();
			 serverIter++)
		{
			for (std::vector<ListenConfig>::iterator lit = serverIter->listenConfigs.begin();
				 lit != serverIter->listenConfigs.end();
				 ++lit)
				dispatch.createListener(lit->IP, lit->port, *serverIter);
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
	}

	if (dispatch.setListeners() == false)
		return (Logger::deleteInstance(), 1);

	// load supported types
	MimeTypes::getInstance()->loadFromFile("mime.types");

	dispatch.loop();

	MimeTypes::deleteInstance();
	LOG_NOTICE("Web server shutting down");
	Logger::deleteInstance();
	return (0);
}
