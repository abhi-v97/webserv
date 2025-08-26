
#include <WebServer.hpp>

void log(const std::string &message)
{
	std::cout << message << std::endl;
}

int main()
{
	WebServer webServer = WebServer("0.0.0.0", 8080);
	webServer.startListen();
	
	return (0);
}