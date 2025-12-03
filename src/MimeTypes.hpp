#pragma once

#include <iostream>
#include <map>

/** \class MimeTypes
 *
 *	Simple class that stores a list of supported file extensions
 */
class MimeTypes
{
public:
	MimeTypes();

	void			  loadFromFile(const std::string &filename);
	std::string		 &getType(const std::string &filename);
	bool			  isSupported(std::string &filename);
	static MimeTypes *getInstance();
	static void		  deleteLogger();

private:
	std::map<std::string, std::string> mSupportedTypes;
	static MimeTypes				  *mInstance;
};
