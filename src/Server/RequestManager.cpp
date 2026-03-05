#include <algorithm>
#include <iostream>
#include <sys/stat.h>

#include "Dispatcher.hpp"
#include "Logger.hpp"
#include "RequestManager.hpp"
#include "RequestParser.hpp"
#include "configParser.hpp"

RequestManager::RequestManager(Dispatcher *dispatch): mDispatch(dispatch)
{
}

// TODO: replace this with a config file setting, use shebang line to execute the file
void isCgi(ServerConfig *srv, RouteResult &out)
{
	LocationConfig &loc = srv->locations[out.locIndex];

	if (loc.cgiEnabled == true)
		out.type = RR_CGI;
}

RouteResult RequestManager::route(RequestParser &parser, ServerConfig *srv, Session *session)
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

bool RequestManager::serveAutoIndex(std::string	  &uri,
									RequestParser &parser,
									ServerConfig  *srv,
									RouteResult	  &out)
{
	std::vector<LocationConfig> &locs = srv->locations;

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
		setError(403, "Auto indexing not allowed for this location", out);
		return (false);
	}
	else
	{
		out.type = RR_AUTOINDEX;
		return (true);
	}
}

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
				return (serveAutoIndex(filePath, parser, srv, out));
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
	if (parser.getMethod() == POST || parser.getMethod() == DELETE)
		return (validateRequest(parser, srv, out));
	setError(404, "File not found", out);
	return (true);
}

// TODO: update this to search deeper than one folder level
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
	out.filePath += uri;
	return (handleCanonicalPath(parser, srv, out));
}

bool RequestManager::validateRequest(RequestParser &parser, ServerConfig *srv, RouteResult &out)
{
	const std::string &uri = out.filePath;
	RequestMethod	   method = parser.getMethod();

	isCgi(srv, out);
	if (!checkMethod(parser, srv, out))
	{
		setError(403, "Method not allowed for this location", out);
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
	if ((method == POST && writePost(uri, parser, out)) ||
		(method == DELETE && deleteMethod(uri, out)))
		return (true);
	return (false);
}

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
	for (int i = 0; i < loc.methods.size(); i++)
	{
		if (loc.methods[i] == methodStr)
			break;
	}
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
	else if (out.type == RR_CGI && access(uri.c_str(), X_OK))
	{
		setError(403, "Failed to execute resource: Missing permissions", out);
		return (false);
	}
	return (true);
}

bool RequestManager::writePost(const std::string &uri, RequestParser &parser, RouteResult &out)
{
	const std::string &bodyFile = parser.getBodyFile();
	bool			   newFile = false;

	if (bodyFile.empty())
		return (true);
	if (access(uri.c_str(), F_OK) == -1)
		newFile = true;
	std::ifstream inf(bodyFile.c_str());
	std::ofstream outf(uri.c_str(), std::ios::app);
	if (!inf || !outf)
	{
		setError(500, "Internal Server Error: failed to write POST message", out);
		return (false);
	}
	outf << inf.rdbuf();
	outf.close();
	std::remove(bodyFile.c_str());
	out.type = RR_BASIC;
	out.bodyMsg = "The requested file \"" + uri.substr(uri.find_last_of('/') + 1) + "\" has been ";
	if (newFile)
		out.bodyMsg += "created.";
	else
		out.bodyMsg += "updated.";
	return (true);
}

bool RequestManager::deleteMethod(const std::string &uri, RouteResult &out)
{
	if (remove(uri.c_str()))
	{
		setError(500, "Internal Server Error: failed to write POST message", out);
		return (false);
	}
	return (true);
}

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
		out.partialLength = std::atoi(rangeStr.c_str() + dash + 1) - out.partialOffset;
		LOG_DEBUG("fileOffset: " + numToString(out.fileOffset) +
				  "; fileLength: " + numToString(out.fileLength));
	}
}

void RequestManager::setError(int status, const std::string &bodyMsg, RouteResult &out)
{
	out.status = status;
	out.bodyMsg = bodyMsg;
	out.type = RR_ERROR;
}
