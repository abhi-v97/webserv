#pragma once

#include <ctime>
#include <string>
#include <sys/types.h>
#include <vector>

/**
 * \class AutoIndex
 *
 * Generates an HTML directory listing page for a given folder path.
 */

 class AutoIndex {
    private:
        std::string buildRow(const std::string& name, bool isDir, off_t size, time_t mtime) const;
        std::string formatSize(off_t size) const;
        std::string formatTime(time_t mtime) const;
    public:
        /** \struct Entry — one item in the directory listing */
        struct Entry
        {
            std::string name;
            bool isDir;
            off_t size;
            time_t mtime;
        };
        
        AutoIndex();
        ~AutoIndex();
        
        std::string generatePage(const std::string& dirPath, const std::string &uri) const;
 };
