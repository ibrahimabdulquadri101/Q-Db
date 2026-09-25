#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <string>
#include <vector>

enum class TokenType
{
	KEYWORD,
	IDENTIFIER,
	INTEGER,
	STRING,
	OPERATOR,
	COMMA,
	LPAREN,
	RPAREN,
	STAR,
	END
};

struct Token
{
	TokenType type;
	std::string value;
};

std::vector<Token> tokenize(std::string input);

#endif // TOKENIZER_H
