#pragma once

#include <cstddef>
#include <string>
#include <unistd.h>

#include "Listener.hpp"
#include "RequestParser.hpp"
#include "configParser.hpp"

struct Session;
class Dispatcher;

/**
	\enum RequestRoute

	Enum that tells ClientHandler how to manage the response, whether to start
	a CGI object or if the response is already set.
*/
enum RequestRoute
{
	RR_NOTSET,
	RR_BASIC,
	RR_GET,
	RR_HEAD,
	RR_PARTIAL,
	RR_CGI,
	RR_CGI_POST,
	RR_ERROR,
};

struct RouteResult
{
	enum RequestRoute type;
	bool			  keepAlive;
	int				  status;
	int				  locIndex;
	size_t			  partialLength;
	size_t			  partialOffset; // use for partial send if request has range header
	std::string		  filePath;
	std::string		  bodyMsg;
	std::string		  sessionId;
};

/**
	\class RequestManager

	Validates and checks a request by cross-referencing with the config file, and returns the result
   as a RouteResult struct
*/
class RequestManager
{
public:
	RequestManager(Dispatcher *dispatch);

	RouteResult route(RequestParser &parser, ServerConfig *srv, Session *session);
	void		setBodyFile(const std::string &bodyFile);

private:
	bool		mIsCgi;
	Dispatcher *mDispatch;

	bool generateResponse();
	bool sendResponse();
	bool writePost(const std::string &uri, RequestParser &parser, RouteResult &out);
	bool deleteMethod(const std::string &uri, RouteResult &out);
	bool checkPermissions(RequestMethod method, RouteResult &out);
	bool validateUri(const std::string &uri,
					 RequestParser	   &parser,
					 ServerConfig	   *srv,
					 RouteResult	   &out);
	bool validateRequest(RequestParser &parser, ServerConfig *srv, RouteResult &out);
	bool checkMethod(RequestParser &parser, ServerConfig *srv, RouteResult &out);
	void setError(int status, const std::string &bodyMsg, RouteResult &out);
	void parseRangeHeader(RequestParser &parser, RouteResult &out);
};
