#include "Parser.h"

#include <cctype>
#include <stdexcept>

namespace
{
const Token &expect(const std::vector<Token> &tokens, std::size_t &position, TokenType type)
{
	if (position >= tokens.size() || tokens[position].type != type)
	{
		throw std::invalid_argument("Unexpected token while parsing statement");
	}
	return tokens[position++];
}

const Token &expectKeyword(const std::vector<Token> &tokens, std::size_t &position, const std::string &keyword)
{
	if (position >= tokens.size() || tokens[position].type != TokenType::KEYWORD)
	{
		throw std::invalid_argument("Expected keyword " + keyword);
	}

	std::string value = tokens[position].value;
	for (char &character : value)
	{
		character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
	}
	if (value != keyword)
	{
		throw std::invalid_argument("Expected keyword " + keyword);
	}
	return tokens[position++];
}

void expectEnd(const std::vector<Token> &tokens, std::size_t position)
{
	if (position >= tokens.size() || tokens[position].type != TokenType::END)
	{
		throw std::invalid_argument("Unexpected trailing tokens");
	}
}
}

SelectStmt parseSelect(std::vector<Token> &tokens)
{
	std::size_t position = 0;
	SelectStmt statement;
	expectKeyword(tokens, position, "SELECT");

	if (position < tokens.size() && tokens[position].type == TokenType::STAR)
	{
		++position;
	}
	else
	{
		statement.columns.push_back(expect(tokens, position, TokenType::IDENTIFIER).value);
		while (position < tokens.size() && tokens[position].type == TokenType::COMMA)
		{
			++position;
			statement.columns.push_back(expect(tokens, position, TokenType::IDENTIFIER).value);
		}
	}

	expectKeyword(tokens, position, "FROM");
	statement.table = expect(tokens, position, TokenType::IDENTIFIER).value;

	if (position < tokens.size() && tokens[position].type == TokenType::KEYWORD)
	{
		expectKeyword(tokens, position, "WHERE");
		statement.where.column = expect(tokens, position, TokenType::IDENTIFIER).value;
		statement.where.op = expect(tokens, position, TokenType::OPERATOR).value;

		if (position >= tokens.size() ||
			(tokens[position].type != TokenType::INTEGER && tokens[position].type != TokenType::STRING))
		{
			throw std::invalid_argument("Expected integer or string WHERE value");
		}
		statement.where.value = tokens[position++].value;
		statement.hasWhere = true;
	}

	expectEnd(tokens, position);
	return statement;
}

InsertStmt parseInsert(std::vector<Token> &tokens)
{
	std::size_t position = 0;
	InsertStmt statement;
	expectKeyword(tokens, position, "INSERT");
	expectKeyword(tokens, position, "INTO");
	statement.table = expect(tokens, position, TokenType::IDENTIFIER).value;
	expectKeyword(tokens, position, "VALUES");
	expect(tokens, position, TokenType::LPAREN);

	if (position < tokens.size() && tokens[position].type != TokenType::RPAREN)
	{
		for (;;)
		{
			if (position >= tokens.size() ||
				(tokens[position].type != TokenType::INTEGER && tokens[position].type != TokenType::STRING))
			{
				throw std::invalid_argument("Expected integer or string INSERT value");
			}
			statement.values.push_back(tokens[position++].value);
			if (position >= tokens.size() || tokens[position].type != TokenType::COMMA)
			{
				break;
			}
			++position;
		}
	}

	expect(tokens, position, TokenType::RPAREN);
	expectEnd(tokens, position);
	return statement;
}
