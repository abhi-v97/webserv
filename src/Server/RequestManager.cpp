#include <algorithm>
#include <iostream>

#include "Dispatcher.hpp"
#include "Logger.hpp"
#include "RequestManager.hpp"
#include "RequestParser.hpp"
#include "configParser.hpp"

RequestManager::RequestManager(Dispatcher *dispatch): mDispatch(dispatch)
{
}

// TODO: replace this with a config file setting, use shebang line to execute the file
bool isCgi(const std::string &uri, RouteResult &out)
{
	if (uri.find(".py") != std::string::npos || uri.find(".sh") != std::string::npos)
	{
		out.type = RR_CGI;
		return (true);
	}
	return (false);
}

RouteResult RequestManager::route(RequestParser &parser, ServerConfig *srv, Session *session)
{
	RouteResult		   result;
	const std::string &uri = parser.getUri();

	result.status = 200;
	result.keepAlive = false;
	result.partialLength = 0;
	result.partialOffset = 0;
	mIsCgi = isCgi(uri, result);
	validateUri(uri, parser, srv, result);
	parseRangeHeader(parser, result);
	return (result);
}

// TODO: update this to search deeper than one folder level
bool RequestManager::validateUri(const std::string &uri,
								 RequestParser	   &parser,
								 ServerConfig	   *srv,
								 RouteResult	   &out)
{
	std::vector<LocationConfig> &locs = srv->locations;

	size_t folderEnd = uri.rfind('/');
	if (folderEnd == std::string::npos || folderEnd == 0)
		folderEnd = 1;

	std::string folder = uri.substr(0, folderEnd);
	for (int j = 0; j < locs.size(); j++)
	{
		if (folder == locs[j].path)
		{
			out.locIndex = j;
			out.filePath = locs[j].root + uri;
			return (validateRequest(parser, srv, out));
		}
	}
	return (false);
}

bool RequestManager::validateRequest(RequestParser &parser, ServerConfig *srv, RouteResult &out)
{
	const std::string &uri = out.filePath;
	RequestMethod	   method = parser.getMethod();

	if (!checkMethod(parser, srv, out))
	{
		setError(403, "Method not allowed for this location", out);
		return (false);
	}
	if (!checkPermissions(parser.getMethod(), out))
		return (false);
	if (mIsCgi == true)
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
		// TODO: test, see what nginx does whena requested file has no read permission
		setError(403, "Failed to read resource", out);
		return (false);
	}
	else if (method == DELETE && access(uri.c_str(), W_OK))
	{
		setError(403, "Failed to delete resource", out);
		return (false);
	}
	else if (mIsCgi == true && access(uri.c_str(), X_OK))
	{
		setError(403, "Failed to execute resource", out);
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
