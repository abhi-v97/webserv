#include "configLexer.hpp"

configLexer::configLexer(const std::string &input) : text(input), pos(0) {

}

configLexer   &configLexer::operator=(const configLexer &other) {
	if (this != &other)
	{
		text = other.text;
		pos = other.pos;
	}
	return *this;
}

configLexer::configLexer(const configLexer &other) {
	*this = other;
}

configLexer::configLexer() : pos(0) {

}

configLexer::~configLexer() {

}

int	configLexer::ft_isalnum(int argument)
{
	if ((argument >= 'a' && argument <= 'z')
		|| (argument >= 'A' && argument <= 'Z')
		|| (argument >= '0' && argument <= '9'))
		return (1);
	else
		return (0);
}

configToken	configLexer::getNextToken()
{
	configToken	Tok;

	skipWhitespaceAndComments();
	char c = text[pos];
	if (pos >= text.size())
	{
		Tok.type = END;
		Tok.value = "";  
	}
	else if (c == '{')
	{
		pos++;
		Tok.type = LBRACE;
		Tok.value = "{"; 
	}
	else if (c == '}')
	{
		pos++;
		Tok.type = RBRACE;
		Tok.value = "}"; 
	}
	else if (c == ';')
	{
		pos++;
		Tok.type = SEMICOLON;
		Tok.value = ";"; 
	}
	else if (ft_isalnum(c) || c == '/' || c == '.' || c == '_' || c == '-' || c == '$')
	{
		size_t start = pos;
		while (pos < text.size() && (ft_isalnum(text[pos]) || text[pos] == '/' || text[pos] == '.' || text[pos] == '_' || text[pos] == '-' || text[pos] == '$'))
			pos++;
		Tok.type = WORD;
		Tok.value = text.substr(start, pos - start); 
	}
	else
		throw std::runtime_error(std::string("Unexpected character: ") + c);
	//std::cout << "Token: " << Tok.value << "\n";
	return  (Tok);
}

void	configLexer::skipWhitespaceAndComments()
{
	while (pos < text.size())
	{
		if (text[pos] == ' ' || text[pos] == '\n' || text[pos] == '\r' || text[pos] == '\t')
			pos++;
		else if (text[pos] == '#')
			while (pos < text.size() && text[pos] != '\n')
				pos++;
		else
			break;
	}
}