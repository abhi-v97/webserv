#include "auto.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <iostream>
#include <string>


bool isDirectoryPath(const std::string &path)
{
    DIR *dir = opendir(path.c_str());
    if (dir == NULL)
        return false;

    closedir(dir);
    return true;
}

DirectoryListing listDirectory(const std::string &dirPath, const std::string &urlPath)
{
    DirectoryListing listing;
    listing.path = dirPath;
    listing.urlPath = urlPath;

    DIR *dir = opendir(dirPath.c_str());
    if (!dir)
        return listing;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        std::string name = entry->d_name;

        if (name == ".")
            continue;
        FileEntry fe;
        fe.name = name;
        std::string fullPath = dirPath;
        if (!fullPath.empty() && fullPath[fullPath.size() - 1] != '/')
            fullPath += "/";
        fullPath += name;

        fe.isDirectory = isDirectoryPath(fullPath);

        listing.entries.push_back(fe);
    }

    closedir(dir);
    return listing;
}

std::string generateAutoindexHTML(const DirectoryListing &listing)
{
    std::string html;

    html += "<html><head><title>Index of ";
    html += listing.urlPath;
    html += "</title></head><body>\n";
    html += "<h1>Index of ";
    html += listing.urlPath;
    html += "</h1>\n<ul>\n";
    html += "<li><a href=\"../\">../</a></li>\n";
    for (size_t i = 0; i < listing.entries.size(); ++i)
    {
        const FileEntry &fe = listing.entries[i];

        html += "<li><a href=\"";
        html += fe.name;
        if (fe.isDirectory)
            html += "/";
        html += "\">";
        html += fe.name;
        if (fe.isDirectory)
            html += "/";
        html += "</a></li>\n";
    }
    html += "</ul></body></html>\n";

    return html;
} 

int main()
{
    std::string dirPath;
    std::cout << "Enter a directory path to test: ";
    std::getline(std::cin, dirPath);
    if (!isDirectoryPath(dirPath))
    {
        std::cout << "Not a directory or cannot open: " << dirPath << std::endl;
        return 1;
    }
    std::string urlPath = "/"; //TODO handle real url path
    DirectoryListing listing = listDirectory(dirPath, urlPath);
    std::cout << "\nEntries found:\n";
    for (size_t i = 0; i < listing.entries.size(); ++i)
    {
        const FileEntry &fe = listing.entries[i];
        std::cout << "  " << fe.name;
        if (fe.isDirectory)
            std::cout << " (dir)";
        std::cout << "\n";
    }
    std::string html = generateAutoindexHTML(listing);
    std::cout << "\nGenerated autoindex HTML:\n\n";
    std::cout << html << std::endl;

    return 0;
}

