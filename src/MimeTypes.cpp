#include <cstddef>

#include "MimeTypes.hpp"
#include "fstream"

// static member init
MimeTypes *MimeTypes::mInstance = NULL;

// in case file parsing fails, treat all files as plain-text
MimeTypes::MimeTypes()
{
	mSupportedTypes["txt"] = "text/plain";
}

/**
 * Returns a singleton instance of MimeTypes, creates one at startup
 */
MimeTypes *MimeTypes::getInstance()
{
	if (mInstance == NULL)
	{
		mInstance = new MimeTypes();
	}
	return mInstance;
}

/**
 * Use to free the singleton instance when shutting down the server
 */
void MimeTypes::deleteLogger()
{
	if (MimeTypes::mInstance)
	{
		delete MimeTypes::mInstance;
		MimeTypes::mInstance = NULL;
	}
}

/**
 * init function, call during server startup to create a map of file extensions and their respective
 * MIME types
 */
void MimeTypes::loadFromFile(const std::string &filename)
{
	std::fstream file("mime.types");

	if (!file.is_open())
		return;
	std::string line;

	for (; std::getline(file, line);)
	{
		if (line.empty() || line.find('#') != std::string::npos)
			continue;

		size_t		divider = line.find(':');
		std::string key = line.substr(0, divider);
		std::string value = line.substr(divider + 1);
		mSupportedTypes[key] = value;
	}
}

/**
 * Returns the MIME-type for a given file
 * Use while building the response, or to determine how to handle a specific file
 */
std::string &MimeTypes::getType(const std::string &filename)
{
	size_t dot = filename.rfind('.');

	if (dot == std::string::npos)
		return (mSupportedTypes["txt"]);

	std::string ext = filename.substr(dot + 1);
	if (mSupportedTypes.find(ext) != mSupportedTypes.end())
		return (mSupportedTypes[ext]);
	else
		return (mSupportedTypes["txt"]);
}

/**
 * Returns true if the file extension is supported.
 *
 * Might be useful.
 */
bool MimeTypes::isSupported(std::string &filename)
{
	size_t dot = filename.rfind('.');

	std::string ext = filename.substr(dot + 1);
	if (mSupportedTypes.find(ext) != mSupportedTypes.end())
		return (true);
	else
		return (false);
}

