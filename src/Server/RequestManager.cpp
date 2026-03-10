#include <algorithm>
#include <iostream>
#include <string>
#include <sys/stat.h>

#include "Dispatcher.hpp"
#include "Logger.hpp"
#include "RequestManager.hpp"
#include "RequestParser.hpp"
#include "configParser.hpp"

RequestManager::RequestManager(Dispatcher *dispatch): mDispatch(dispatch)
{
}

/**
	\brief Helper function, sets CGI as output result if cgi is enabled for this location

	\param srv ServerConfig object of the server block
	\param out RouteResult struct sent back to ClientHandler
*/
void isCgi(const std::string &uri, RequestParser &parser, ServerConfig *srv, RouteResult &out)
{
	LocationConfig &loc = srv->locations[out.locIndex];
	size_t extStart = uri.find_last_of('.');
	RequestMethod  method = parser.getMethod();
	std::string methodStr;

	if (method == POST)
		methodStr = "POST";
	else if (method == DELETE)
		methodStr = "DELETE";
	else if (method == GET)
		methodStr = "GET";
	else if (method == HEAD)
		methodStr = "HEAD";
	if (extStart == std::string::npos)
		return;
	std::string ext = uri.substr(extStart);
	for (std::vector<CgiConfig>::const_iterator it = loc.cgis.begin(); it != loc.cgis.end(); it++)
	{
		if (ext == it->extension)
		{
			for (std::vector<std::string>::const_iterator itt = it->methods.begin(); itt != it->methods.end(); itt++)
			{
				if (*itt == methodStr)
					out.type = RR_CGI;
			}
		}
	}
}

/**
	\brief Routes the request through a series of handlers and returns a copy of the struct to
   ClientHandler so that it can send the correct response

   \param parser RequestParser object which contains certain request info
   \param srv ServerConfig object of the server block
*/
RouteResult RequestManager::route(RequestParser &parser, ServerConfig *srv)
{
	RouteResult	 result;
	std::string &uri = parser.getUri();

	result.status = 200;
	result.keepAlive = false;
	result.partialLength = 0;
	result.partialOffset = 0;
	validateUri(uri, parser, srv, result);
	parseRangeHeader(parser, result);
	return (result);
}

/**
	\brief Handles indexing resopnse

	\param parser RequestParser object which contains certain request info
	\param srv ServerConfig object of the server block
	\param out reference to the RouteResult struct to be sent back to client

	Tries to first check for a given index file in the location block. If not found checks if
   auto-indexing is valid for this location and returns the appropriate response
*/
bool RequestManager::handleIndex(RequestParser &parser, ServerConfig *srv, RouteResult &out)
{
	std::vector<LocationConfig> &locs = srv->locations;

	isCgi(out.filePath, parser, srv, out);
	if (!checkMethod(parser, srv, out))
	{
		setError(405, "Method not allowed for this location", out);
		return (false);
	}
	if (!locs[out.locIndex].index.empty())
	{
		std::string indexFile = out.filePath + locs[out.locIndex].index;
		if (!access(indexFile.c_str(), F_OK))
		{
			out.filePath.clear();
			out.filePath = indexFile;
			return (validateRequest(parser, srv, out));
		}
	}
	if (locs[out.locIndex].autoindex == false)
	{
		setError(404, "Index file not found", out);
		return (false);
	}
	else
	{
		out.type = RR_AUTOINDEX;
		return (true);
	}
}

/**
	\brief Determines the canonical path of the request URI

	\param parser RequestParser object which contains certain request info
	\param srv ServerConfig object of the server block
	\param out reference to the RouteResult struct to be sent back to client

	\details Handles rerouting if the given URI requested a folder, or vice versa, which is done by
   checking if the URI ends in a slash. Handles the scenarios like a folder that doesn't end in a
   slash, a file URI that ends in a slash etc.
*/
bool RequestManager::handleCanonicalPath(RequestParser &parser, ServerConfig *srv, RouteResult &out)
{
	std::vector<LocationConfig> &locs = srv->locations;
	bool						 hasSlash = false;
	std::string					&filePath = out.filePath;

	std::string::const_iterator it = filePath.end();
	if (!filePath.empty())
		it--;
	if (*it == '/')
		hasSlash = true;

	if (parser.getMethod() == POST || parser.getMethod() == DELETE)
	{
		if (hasSlash == true)
			filePath.erase(filePath.size() - 1, 1);
		return (validateRequest(parser, srv, out));
	}
	struct stat st;
	if (stat(filePath.c_str(), &st) == 0)
	{
		if (S_ISDIR(st.st_mode))
		{
			if (hasSlash == false)
			{
				out.filePath = parser.getUri();
				out.filePath += '/';
				if (parser.getMethod() == GET)
					out.status = 301;
				else
					out.status = 307;
				out.type = RR_REDIRECT;
				return (true);
			}
			else
				return (handleIndex(parser, srv, out));
		}
		else
		{
			if (hasSlash == true && S_ISREG(st.st_mode))
			{
				out.filePath = parser.getUri();
				out.filePath.erase(filePath.size() - 1, 1);
				if (parser.getMethod() == GET)
					out.status = 301;
				else
					out.status = 307;
				out.type = RR_REDIRECT;
				return (true);
			}
			else
				return (validateRequest(parser, srv, out));
		}
	}
	else
	{
		// if stat fails
		if (!filePath.empty() && hasSlash == true)
		{
			filePath.erase(filePath.size() - 1, 1);
			struct stat st2;
			if (stat(filePath.c_str(), &st2) == 0 && S_ISREG(st2.st_mode))
			{
				out.filePath = parser.getUri();
				out.filePath.erase(filePath.size() - 1, 1);
				if (parser.getMethod() == GET)
					out.status = 301;
				else
					out.status = 307;
				out.type = RR_REDIRECT;
				return (true);
			}
		}
	}
	setError(404, "File not found", out);
	return (true);
}

/**
	\brief Finds the location block which best matches the given URI

	\param uri Reference to request URI
	\param parser RequestParser object which contains certain request info
	\param srv ServerConfig object of the server block
	\param out reference to the RouteResult struct to be sent back to client

	\details The function looks through all available locations for a perfect match, and if not
   found, settles on the best match which is simply the longest matching string.
*/
bool RequestManager::validateUri(std::string   &uri,
								 RequestParser &parser,
								 ServerConfig  *srv,
								 RouteResult   &out)
{
	std::vector<LocationConfig> &locs = srv->locations;

	if (uri.empty())
		uri = "/";
	// ensure uri starts with '/' for comparison
	if (uri[0] != '/')
		uri = std::string("/") + uri;

	int winner = 0;
	int bestMatch = 0;

	for (int i = 0; i < static_cast<int>(locs.size()); i++)
	{
		const std::string &lp = locs[i].path;
		if (lp.empty())
			continue;

		// if uri == lp or uri has lp in prefix
		if (uri.compare(0, lp.size(), lp) == 0)
		{
			if (uri.size() == lp.size() || (uri.size() > lp.size() && uri[lp.size()] == '/'))
			{
				if (lp.size() > bestMatch)
				{
					bestMatch = lp.size();
					winner = i;
				}
			}
		}
	}
	out.locIndex = winner;
	out.filePath = srv->root;
	// avoid duplicate slashes
	if (!out.filePath.empty() && uri.at(0) != '/' &&
		out.filePath.at(out.filePath.size() - 1) != '/')
		out.filePath += '/';
	if (!locs[winner].alias.empty())
	{
		out.filePath = locs[winner].alias;
		out.filePath += uri.substr(locs[winner].path.size());
	}
	else
		out.filePath += uri;
	return (handleCanonicalPath(parser, srv, out));
}

/**
	\brief Verifies request method and file permissions

	\param parser RequestParser object which contains certain request info
	\param srv ServerConfig object of the server block
	\param out reference to the RouteResult struct to be sent back to client
*/
bool RequestManager::validateRequest(RequestParser &parser, ServerConfig *srv, RouteResult &out)
{
	const std::string &uri = out.filePath;
	RequestMethod	   method = parser.getMethod();

	isCgi(out.filePath, parser, srv, out);
	if (!checkMethod(parser, srv, out))
	{
		setError(405, "Method not allowed for this location", out);
		return (false);
	}
	if (!checkPermissions(parser.getMethod(), out))
		return (false);
	if (out.type == RR_CGI)
	{
		if (method == POST)
		{
			out.type = RR_CGI_POST;
			return (true);
		}
		return (true);
	}
	if (method == GET)
	{
		out.type = RR_GET;
		return (true);
	}
	if ((method == POST && writePost(parser, out)) || (method == DELETE && deleteMethod(out)))
		return (true);
	return (false);
}

/**
	\brief Verifies the request method for the given location block

	\param parser RequestParser object which contains certain request info
	\param srv ServerConfig object of the server block
	\param out reference to the RouteResult struct to be sent back to client
*/
bool RequestManager::checkMethod(RequestParser &parser, ServerConfig *srv, RouteResult &out)
{
	std::string	   methodStr;
	RequestMethod  method = parser.getMethod();
	LocationConfig loc = srv->locations[out.locIndex];

	if (method == POST)
		methodStr = "POST";
	else if (method == DELETE)
		methodStr = "DELETE";
	else if (method == GET)
		methodStr = "GET";
	else if (method == HEAD)
		methodStr = "HEAD";
	for (int i = 0; i < loc.methods.size(); i++)
	{
		if (loc.methods[i] == methodStr)
			break;
	}
	// if (method == POST)
	// {
	// 	int size = atoi(parser.getHeaders()["content-length"].c_str());
		
	// 	if (size == 0)
	// 	{
	// 		setError(405, "Bad Request", out);
	// 		return (true);
	// 	}
	// }
	if (find(loc.methods.begin(), loc.methods.end(), methodStr) == loc.methods.end())
		return (false);
	return (true);
}

/**
	\brief checks the full uri if the file has correct permissions

	\param uri must be the full path of the uri
	\param method request method
*/
bool RequestManager::checkPermissions(RequestMethod method, RouteResult &out)
{
	const std::string &uri = out.filePath;

	if (access(uri.c_str(), F_OK))
	{
		if (method == GET || method == DELETE)
		{
			setError(404, "Page not found", out);
			return (false);
		}
		else if (method == POST)
		{
			// POST method, okay if file doesn't exist
			// 201: file created status
			out.status = 201;
			return (true);
		}
	}
	else if (method == GET && access(uri.c_str(), R_OK))
	{
		setError(403, "Failed to read resource: Missing permissions", out);
		return (false);
	}
	else if (method == DELETE && access(uri.c_str(), W_OK))
	{
		setError(403, "Failed to delete resource: Missing permissions", out);
		return (false);
	}
	// else if (out.type == RR_CGI && access(uri.c_str(), X_OK))
	// {
	// 	setError(403, "Failed to execute resource: Missing permissions", out);
	// 	return (false);
	// }
	return (true);
}

/**
	\brief attempts to perform the write operation for a POST request

	\param srv ServerConfig object of the server block
	\param out reference to the RouteResult struct to be sent back to client
*/
bool RequestManager::writePost(RequestParser &parser, RouteResult &out)
{
	const std::string &bodyFile = parser.getBodyFile();
	bool			   newFile = false;

	if (bodyFile.empty())
		return (true);
	if (access(out.filePath.c_str(), F_OK) == -1)
		newFile = true;
	std::ifstream inf(bodyFile.c_str());
	std::ofstream outf(out.filePath.c_str(), std::ios::app);
	if (!inf || !outf)
	{
		setError(500, "Internal Server Error: failed to write POST message", out);
		return (false);
	}
	outf << inf.rdbuf();
	outf.close();
	std::remove(bodyFile.c_str());
	out.type = RR_BASIC;
	out.bodyMsg = "The requested file \"" +
				  out.filePath.substr(out.filePath.find_last_of('/') + 1) + "\" has been ";
	if (newFile)
		out.bodyMsg += "created.";
	else
		out.bodyMsg += "updated.";
	return (true);
}

/**
	\brief attempts to perform the delete operation for a DELETE request

	\param srv ServerConfig object of the server block
	\param out reference to the RouteResult struct to be sent back to client
*/
bool RequestManager::deleteMethod(RouteResult &out)
{
	if (remove(out.filePath.c_str()))
	{
		setError(500, "Internal Server Error: failed to write POST message", out);
		return (false);
	}
	return (true);
}

/**
	\brief Searches for and handles the range header field

	\param parser RequestParser object which contains certain request info
	\param out reference to the RouteResult struct to be sent back to client

	\details If a HTTP request contains the range header field, its a partial request which needs
	specific bytes of a file, eg a video file.
	Example: Range: bytes=0-1023
	If range-end isn't provided, assume it to be the end of file
*/
void RequestManager::parseRangeHeader(RequestParser &parser, RouteResult &out)
{
	std::string rangeStr = parser.getHeaders()["range"];

	if (rangeStr.empty())
		return;
	else
	{
		out.status = 206;
		out.type = RR_PARTIAL;
		size_t equal = rangeStr.find_first_of('=');
		size_t dash = rangeStr.find_first_of('-');

		out.partialOffset = std::atoi(rangeStr.c_str() + equal + 1);
		ssize_t rangeEnd = -1;
		rangeEnd = std::atoi(rangeStr.c_str() + dash + 1);
		if (rangeEnd != -1)
			out.partialLength = std::atoi(rangeStr.c_str() + dash + 1) - out.partialOffset;
		LOG_DEBUG("fileOffset: " + numToString(out.partialOffset) +
				  "; fileLength: " + numToString(out.partialLength));
	}
}

/**
	\brief Helper function, used to set error info in RouteResult struct
	
	\param status HTTP status code
	\param bodyMsg Error message, to be sent as response body
	\param out reference to the RouteResult struct to be sent back to client
*/
void setError(int status, const std::string &bodyMsg, RouteResult &out)
{
	out.status = status;
	out.bodyMsg = bodyMsg;
	out.type = RR_ERROR;
}
