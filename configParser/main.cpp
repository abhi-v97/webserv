#include "configParser.hpp"

int main()
{
    try
    {
        configParser parser("input.txt");

        for (size_t s = 0; s < parser.servers.size(); s++)
        {
            const ServerConfig& srv = parser.servers[s];
            std::cout << "Server " << s+1 << ":\n";
            std::cout << "  Root: " << srv.root << "\n";
            std::cout << "  Ports: ";
            for (size_t p = 0; p < srv.listenPorts.size(); p++)
                std::cout << srv.listenPorts[p] << " ";
            std::cout << "\n";
			std::cout << "  Error pages: ";
			for(std::map<int, std::string>::const_iterator it = srv.errorPages.begin();
 			   it != srv.errorPages.end(); ++it)
			{
    			std::cout << it->first << " " << it->second << "\n";
			}
            for (size_t l = 0; l < srv.locations.size(); l++)
            {
                const LocationConfig& loc = srv.locations[l];
                std::cout << "  Location " << loc.path << ":\n";
				std::cout << "    Index: " << loc.index << "\n";
                std::cout << "    Root: " << loc.root << "\n";
                std::cout << "    Autoindex: " << (loc.autoindex ? "on" : "off") << "\n";
                std::cout << "    Methods: ";
                for (size_t m = 0; m < loc.methods.size(); m++)
                    std::cout << loc.methods[m] << " ";
                std::cout << "\n";
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

