#ifndef PARSER_H
#define PARSER_H

#include "ColumnDef.h"
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


struct CreateTableStmt
{
	std::string table;
	std::vector<ColumnDef> columns;
};

struct DeleteStmt
{
	std::string table;
	bool hasWhere = false;
	WhereClause where;
};

struct UpdateAssignment
{
	std::string column;
	std::string value;
};

struct UpdateStmt
{
	std::string table;
	std::vector<UpdateAssignment> assignments;
	bool hasWhere = false;
	WhereClause where;
};

struct DropTableStmt
{
	std::string table;
	bool ifExists = false; // IF EXISTS suppresses error when table not found
};

SelectStmt parseSelect(std::vector<Token> &tokens);
InsertStmt parseInsert(std::vector<Token> &tokens);
CreateTableStmt parseCreateTable(std::vector<Token> &tokens);
DeleteStmt parseDelete(std::vector<Token> &tokens);
UpdateStmt parseUpdate(std::vector<Token> &tokens);
DropTableStmt parseDropTable(std::vector<Token> &tokens);

#endif // PARSER_H
