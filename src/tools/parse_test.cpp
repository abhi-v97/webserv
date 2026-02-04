#include <iostream>
#include "configParser.hpp"

static void printLocation(const LocationConfig &loc)
{
    std::cout << "  location: path=" << loc.path << "\n";
    std::cout << "    root=" << loc.root << "\n";
    std::cout << "    autoindex=" << (loc.autoindex ? "on" : "off") << "\n";
    std::cout << "    index=" << loc.index << "\n";
    std::cout << "    uploadDir=" << loc.uploadDir << "\n";
    std::cout << "    redirect=" << loc.redirect << " code=" << loc.redirectErr << "\n";
    std::cout << "    methods:";
    for (size_t i = 0; i < loc.methods.size(); ++i) std::cout << " " << loc.methods[i];
    std::cout << "\n";
}

int main(int argc, char **argv)
{
    std::string cfg = (argc > 1) ? argv[1] : "default.conf";
    try {
        configParser parser(cfg);
        std::cout << "Servers parsed: " << parser.servers.size() << "\n";
        for (size_t i = 1; i < parser.servers.size(); ++i) {
            const ServerConfig &s = parser.servers[i];
            std::cout << "Server " << i << ":\n";
            std::cout << " serverName=" << s.serverName << "\n";
            std::cout << " root=" << s.root << "\n";
            std::cout << " clientMaxBodySize=" << s.clientMaxBodySize << "\n";
            std::cout << " listenPorts:";
            for (size_t j = 0; j < s.listenPorts.size(); ++j) std::cout << " " << s.listenPorts[j];
            std::cout << "\n";
            std::cout << " errorPages:\n";
            for (std::map<int,std::string>::const_iterator it = s.errorPages.begin(); it != s.errorPages.end(); ++it)
                std::cout << "   " << it->first << " -> " << it->second << "\n";
            std::cout << " locations: " << s.locations.size() << "\n";
            for (size_t k = 0; k < s.locations.size(); ++k) printLocation(s.locations[k]);
        }
    } catch (const std::exception &e) {
        std::cerr << "Parse error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}