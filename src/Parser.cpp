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
			(tokens[position].type != TokenType::INTEGER && tokens[position].type != TokenType::STRING && tokens[position].type != TokenType::IDENTIFIER))
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

CreateTableStmt parseCreateTable(std::vector<Token> &tokens)
{
	std::size_t position = 0;
	CreateTableStmt statement;
	expectKeyword(tokens, position, "CREATE");
	expectKeyword(tokens, position, "TABLE");
	statement.table = expect(tokens, position, TokenType::IDENTIFIER).value;
	expect(tokens, position, TokenType::LPAREN);

	while (position < tokens.size() && tokens[position].type != TokenType::RPAREN)
	{
		ColumnDef col;
		col.name = expect(tokens, position, TokenType::IDENTIFIER).value;

		// Type must be a KEYWORD (INT or CHAR)
		if (position >= tokens.size() || tokens[position].type != TokenType::KEYWORD)
		{
			throw std::invalid_argument("Expected column type (INT or CHAR) after column name");
		}
		std::string typeName = tokens[position++].value;
		for (char &c : typeName)
		{
			c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
		}
		if (typeName != "INT" && typeName != "CHAR")
		{
			throw std::invalid_argument("Unknown column type: " + typeName + ". Expected INT or CHAR");
		}
		col.type = typeName;
		statement.columns.push_back(col);

		if (position < tokens.size() && tokens[position].type == TokenType::COMMA)
		{
			++position;
		}
	}

	expect(tokens, position, TokenType::RPAREN);
	expectEnd(tokens, position);

	if (statement.columns.empty())
	{
		throw std::invalid_argument("CREATE TABLE requires at least one column definition");
	}
	return statement;
}

DeleteStmt parseDelete(std::vector<Token> &tokens)
{
	std::size_t position = 0;
	DeleteStmt statement;
	expectKeyword(tokens, position, "DELETE");
	expectKeyword(tokens, position, "FROM");
	statement.table = expect(tokens, position, TokenType::IDENTIFIER).value;

	if (position < tokens.size() && tokens[position].type == TokenType::KEYWORD)
	{
		expectKeyword(tokens, position, "WHERE");
		statement.where.column = expect(tokens, position, TokenType::IDENTIFIER).value;
		statement.where.op = expect(tokens, position, TokenType::OPERATOR).value;

		if (position >= tokens.size() ||
			(tokens[position].type != TokenType::INTEGER && tokens[position].type != TokenType::STRING && tokens[position].type != TokenType::IDENTIFIER))
		{
			throw std::invalid_argument("Expected integer or string WHERE value");
		}
		statement.where.value = tokens[position++].value;
		statement.hasWhere = true;
	}

	expectEnd(tokens, position);
	return statement;
}

UpdateStmt parseUpdate(std::vector<Token> &tokens)
{
	std::size_t position = 0;
	UpdateStmt statement;
	expectKeyword(tokens, position, "UPDATE");
	statement.table = expect(tokens, position, TokenType::IDENTIFIER).value;
	expectKeyword(tokens, position, "SET");

	while (true)
	{
		UpdateAssignment assignment;
		assignment.column = expect(tokens, position, TokenType::IDENTIFIER).value;

		const Token &opToken = expect(tokens, position, TokenType::OPERATOR);
		if (opToken.value != "=")
		{
			throw std::invalid_argument("Expected '=' in UPDATE SET assignment");
		}

		if (position >= tokens.size() ||
			(tokens[position].type != TokenType::INTEGER && tokens[position].type != TokenType::STRING && tokens[position].type != TokenType::IDENTIFIER))
		{
			throw std::invalid_argument("Expected value in UPDATE SET assignment");
		}
		assignment.value = tokens[position++].value;
		statement.assignments.push_back(assignment);

		if (position < tokens.size() && tokens[position].type == TokenType::COMMA)
		{
			++position;
			continue;
		}
		break;
	}

	if (statement.assignments.empty())
	{
		throw std::invalid_argument("UPDATE requires at least one SET assignment");
	}

	if (position < tokens.size() && tokens[position].type == TokenType::KEYWORD)
	{
		expectKeyword(tokens, position, "WHERE");
		statement.where.column = expect(tokens, position, TokenType::IDENTIFIER).value;
		statement.where.op = expect(tokens, position, TokenType::OPERATOR).value;

		if (position >= tokens.size() ||
			(tokens[position].type != TokenType::INTEGER && tokens[position].type != TokenType::STRING && tokens[position].type != TokenType::IDENTIFIER))
		{
			throw std::invalid_argument("Expected integer or string WHERE value");
		}
		statement.where.value = tokens[position++].value;
		statement.hasWhere = true;
	}

	expectEnd(tokens, position);
	return statement;
}


DropTableStmt parseDropTable(std::vector<Token> &tokens)
{
	std::size_t position = 0;
	DropTableStmt statement;
	expectKeyword(tokens, position, "DROP");
	expectKeyword(tokens, position, "TABLE");

	// Optional: IF EXISTS
	if (position < tokens.size() &&
		tokens[position].type == TokenType::KEYWORD &&
		tokens[position].value == "IF")
	{
		++position; // consume IF
		if (position < tokens.size() &&
			tokens[position].type == TokenType::KEYWORD &&
			tokens[position].value == "EXISTS")
		{
			++position; // consume EXISTS
			statement.ifExists = true;
		}
		else
		{
			throw std::invalid_argument("Expected EXISTS after IF in DROP TABLE");
		}
	}

	// Table name is an IDENTIFIER
	if (position >= tokens.size() || tokens[position].type != TokenType::IDENTIFIER)
	{
		throw std::invalid_argument("Expected table name in DROP TABLE");
	}
	statement.table = tokens[position++].value;
	expectEnd(tokens, position);
	return statement;
}
