#ifndef AUTO_HPP
#define AUTO_HPP

#include <string>
#include <vector>

struct FileEntry {
    std::string name;
    bool        isDirectory;
};

struct DirectoryListing {
    std::string          path;
    std::string          urlPath;
    std::vector<FileEntry> entries;
};

DirectoryListing listDirectory(const std::string &dirPath, const std::string &urlPath);

std::string generateAutoindexHTML(const DirectoryListing &listing);

#endif
