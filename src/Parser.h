#ifndef PARSER_H
#define PARSER_H

#include "Tokenizer.h"

struct WhereClause
{
	std::string column;
	std::string op;
	std::string value;
};

struct SelectStmt
{
	std::vector<std::string> columns;
	std::string table;
	bool hasWhere = false;
	WhereClause where;
};

struct InsertStmt
{
	std::string table;
	std::vector<std::string> values;
};

SelectStmt parseSelect(std::vector<Token> &tokens);
InsertStmt parseInsert(std::vector<Token> &tokens);

#endif // PARSER_H
