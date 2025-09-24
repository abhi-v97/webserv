#ifndef CONFIGLEXER_HPP
# define CONFIGLEXER_HPP

#include <string>
#include <stdexcept>
#include <iostream>

enum configTokenType {
	WORD,
	LBRACE,
	RBRACE,
	SEMICOLON,
	END
};

struct configToken {
	configTokenType	type;
	std::string		value;
};

class configLexer {
	private:
		std::string	text;
		size_t		pos;
		void		skipWhitespaceAndComments();
		int			ft_isalnum(int argument);
	public:
		configLexer(const std::string &input);
		configLexer &operator=(const configLexer &other);
		configLexer(const configLexer &other);
		configLexer();
		~configLexer();

		configToken   getNextToken();
};

#endif