#include "AutoIndex.hpp"
#include "../Logger.hpp"

#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <vector>
#include <algorithm>
#include <dirent.h>
#include <cerrno>
#include <cstring>

AutoIndex::AutoIndex() {}

AutoIndex::~AutoIndex() {}

/**
 * \brief Generates a complete HTML page listing all entries in dirPath.
 *
 * \param dirPath  Absolute or relative filesystem path to the directory
 *                 (e.g. "www/uploads/")
 * \param uri      The URI as requested by the browser
 *                 (e.g. "/uploads/") – used for display and link building
 * \return         A full HTTP-ready HTML string, or an empty string on error.
 */

std::string AutoIndex::generatePage(const std::string& dirPath, const std::string &uri) const {
    LOG_DEBUG("AutoIndex::generatePage called for dir=" + dirPath + " uri=" + uri);

    DIR *dir = opendir(dirPath.c_str());
    if (!dir) {
        LOG_ERROR("Autoindex: opendir failed for \"" + dirPath + "\": " + std::strerror(errno));
        return ("");
    }
        

    //collect entries in a vector contained so we can sort them alphabetically
    std::vector<Entry> entries;

    struct dirent *dp;
    while ((dp = readdir(dir)) != NULL) {
        std::string name = dp->d_name;

        // hide current directory entry "." and keep parent ".." so we can navigate up
        if (name == ".")
            continue;
        
        std::string fullPath = dirPath;
        if (!fullPath.empty() && fullPath[fullPath.size() - 1] != '/')
            fullPath += '/';
        fullPath += name;

        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) {
            LOG_WARNING("Autoindex: stat failed for \"" + fullPath + "\": " + std::strerror(errno));
            continue;
        }
        Entry e;
        e.name = name;
        e.isDir = S_ISDIR(st.st_mode);
        e.mtime = st.st_mtime;
        e.size = st.st_size;
        entries.push_back(e);
    }
    closedir(dir);
    
    LOG_DEBUG("AutoIndex: found " + numToString(entries.size()) + " entries in " + dirPath );

    // sort: directories first then alphabetically withing each group (dir/dir/file)
    for (size_t i = 0; i < entries.size(); i++) {
        for (size_t j = i + 1; j < entries.size(); j++) {
            bool swapNeeded = false;
            if (entries[j].isDir && !entries[i].isDir)
                swapNeeded = true;
            else if(entries[i].isDir == entries[j].isDir && entries[i].name > entries[j].name)
                swapNeeded = true;
            if (swapNeeded)
                std::swap(entries[i], entries[j]);
        }
    }

    // make sure the URI ends with '/' for correct relative links
    std::string baseUri = uri;
    if (!baseUri.empty() && baseUri[baseUri.length() - 1] != '/')
        baseUri += '/';

    // build the HTML page
    std::ostringstream html;

    html << "<!DOCTYPE html>\n"
        << "<html lang=\"en\">\n"
        << "<head>\n"
        << "  <meta charset=\"UTF-8\">\n"
        << "  <title>Index of " << baseUri << "</title>\n"
        << "  <style>\n"
        << "    body { font-family: monospace; margin: 2em; background:#f9f9f9; color:#333; }\n"
        << "    h1   { border-bottom: 1px solid #ccc; padding-bottom: 0.3em; }\n"
        << "    table{ border-collapse: collapse; width: 100%; }\n"
        << "    th, td { text-align: left; padding: 4px 12px; }\n"
        << "    th   { border-bottom: 2px solid #aaa; }\n"
        << "    tr:hover { background: #eef; }\n"
        << "    a    { text-decoration: none; color: #0057b8; }\n"
        << "    a:hover { text-decoration: underline; }\n"
        << "    .size{ text-align: right; }\n"
        << "  </style>\n"
        << "</head>\n"
        << "<body>\n"
        << "  <h1>Index of " << baseUri << "</h1>\n"
        << "  <table>\n"
        << "    <tr><th>Name</th><th>Last modified</th><th class=\"size\">Size</th></tr>\n";

    for (size_t i = 0; i < entries.size(); i++) {
        html << buildRow(entries[i].name, entries[i].isDir, entries[i].size, entries[i].mtime);
    }

    html << "  </table>\n"
        << "  <hr>\n"
        << "  <small>webserv</small>\n"
        << "</body>\n"
        << "</html>\n";
    
    LOG_INFO("AutoIndex: generated HTML page for \"" + baseUri + "\" (" + numToString(entries.size()) + " entries)");
    
    return html.str();
}

/**
 * \brief Builds a single table row for one directory entry.
 */

std::string AutoIndex::buildRow(const std::string& name, bool isDir, off_t size, time_t mtime) const { 
    std::ostringstream row;
    std::string displayName = name + (isDir ? "/" : ""); 
    std::string href = name + (isDir ? "/" : "");

    row << "    <tr>"
		<< "<td><a href=\"" << href << "\">" << displayName << "</a></td>"
		<< "<td>" << formatTime(mtime) << "</td>"
		<< "<td class=\"size\">" << (isDir ? "-" : formatSize(size)) << "</td>"
		<< "</tr>\n";
    
    return (row.str());
}

/**
 * \brief Formats a byte count into a human-readable string (B, KB, MB, GB).
 */
std::string AutoIndex::formatSize(off_t size) const {
    std::ostringstream oss;

    if (size < 1024) 
        oss << size << " B";
    else if (size < 1024 * 1024) 
        oss << std::fixed << std::setprecision(1) << (double)size / 1024.0 << " KB";
    else if (size < 1024 * 1024 * 1024) 
        oss << std::fixed << std::setprecision(1) << (double)size / (1024.0 * 1024.0) << " MB";
    else 
        oss << std::fixed << std::setprecision(1) << (double)size / (1024.0 * 1024.0 * 1024.0) << " GB";
    return oss.str();
}

/**
 * \brief Formats a time_t into "YYYY-MM-DD HH:MM" string.
 */
std::string AutoIndex::formatTime(time_t mtime) const {
    char buf[32];
    struct tm *tm_info = localtime(&mtime);

    if (!tm_info)
        return "-";
    strftime(buf, sizeof(buf), "%Y-%m-%d, %H:%M", tm_info);
    return (std::string(buf));
}

