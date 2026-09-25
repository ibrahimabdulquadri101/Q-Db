#include "Executor.h"

#include "Catalog.h"
#include "DiskManager.h"
#include "Row.h"
#include "Table.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
std::string uppercase(std::string value)
{
	for (char &character : value)
	{
		character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
	}
	return value;
}

int32_t parseInteger(const std::string &value)
{
	std::size_t parsed = 0;
	long long result;
	try
	{
		result = std::stoll(value, &parsed);
	}
	catch (const std::exception &)
	{
		throw std::invalid_argument("Expected a 32-bit integer value");
	}
	if (parsed != value.size() || result < std::numeric_limits<int32_t>::min() ||
		result > std::numeric_limits<int32_t>::max())
	{
		throw std::invalid_argument("Expected a 32-bit integer value");
	}
	return static_cast<int32_t>(result);
}

std::string fieldValue(const Row &row, const std::string &column)
{
	const std::string name = uppercase(column);
	if (name == "ID") return std::to_string(row.id);
	if (name == "NAME") return row.name;
	if (name == "AGE") return std::to_string(row.age);
	throw std::invalid_argument("Unknown column: " + column);
}

bool matches(const Row &row, const WhereClause &where)
{
	const std::string column = uppercase(where.column);
	if (where.op != "=" && where.op != "<" && where.op != ">")
	{
		throw std::invalid_argument("Unsupported comparison operator: " + where.op);
	}

	if (column == "ID" || column == "AGE")
	{
		const int32_t actual = column == "ID" ? row.id : row.age;
		const int32_t expected = parseInteger(where.value);
		if (where.op == "=") return actual == expected;
		if (where.op == "<") return actual < expected;
		return actual > expected;
	}

	if (column == "NAME")
	{
		const std::string actual(row.name);
		if (where.op == "=") return actual == where.value;
		if (where.op == "<") return actual < where.value;
		return actual > where.value;
	}

	throw std::invalid_argument("Unknown column: " + where.column);
}
}

Executor::Executor(Catalog *catalog, DiskManager *disk) : catalog(catalog), disk(disk) {}

void Executor::execute(std::string sql)
{
	try
	{
		if (catalog == nullptr || disk == nullptr)
		{
			throw std::runtime_error("Executor is not initialized");
		}

		std::vector<Token> tokens = tokenize(std::move(sql));
		if (tokens.empty() || tokens.front().type != TokenType::KEYWORD)
		{
			std::cout << "Unknown command\n";
			return;
		}

		const std::string command = uppercase(tokens.front().value);
		if (command == "SELECT")
		{
			SelectStmt statement = parseSelect(tokens);
			executeSelect(statement);
		}
		else if (command == "INSERT")
		{
			InsertStmt statement = parseInsert(tokens);
			executeInsert(statement);
		}
		else
		{
			std::cout << "Unknown command\n";
		}
	}
	catch (const std::exception &error)
	{
		std::cerr << "Execution error: " << error.what() << '\n';
	}
}

void Executor::executeInsert(InsertStmt &stmt)
{
	if (catalog == nullptr || disk == nullptr)
	{
		throw std::runtime_error("Executor is not initialized");
	}
	Table *table = catalog->getTable(stmt.table);
	if (table == nullptr)
	{
		throw std::invalid_argument("Table not found: " + stmt.table);
	}
	if (stmt.values.size() != 3)
	{
		throw std::invalid_argument("INSERT requires values for id, name, and age");
	}
	if (stmt.values[1].size() >= sizeof(Row::name))
	{
		throw std::invalid_argument("Name must be shorter than 32 characters");
	}

	Row row{};
	row.id = parseInteger(stmt.values[0]);
	std::memcpy(row.name, stmt.values[1].data(), stmt.values[1].size());
	row.age = parseInteger(stmt.values[2]);
	if (!table->insert(row))
	{
		throw std::runtime_error("Failed to insert row into table: " + stmt.table);
	}
	std::cout << "Inserted 1 row\n";
}

void Executor::executeSelect(SelectStmt &stmt)
{
	if (catalog == nullptr || disk == nullptr)
	{
		throw std::runtime_error("Executor is not initialized");
	}
	Table *table = catalog->getTable(stmt.table);
	if (table == nullptr)
	{
		throw std::invalid_argument("Table not found: " + stmt.table);
	}

	for (const std::string &column : stmt.columns)
	{
		const std::string normalized = uppercase(column);
		if (normalized != "ID" && normalized != "NAME" && normalized != "AGE")
		{
			throw std::invalid_argument("Unknown column: " + column);
		}
	}
	if (stmt.hasWhere)
	{
		matches(Row{}, stmt.where);
	}

	std::size_t count = 0;
	table->scan([&](const Row &row) {
		if (stmt.hasWhere && !matches(row, stmt.where))
		{
			return;
		}

		if (stmt.columns.empty())
		{
			std::cout << row.id << ',' << row.name << ',' << row.age << '\n';
		}
		else
		{
			for (std::size_t i = 0; i < stmt.columns.size(); ++i)
			{
				if (i != 0) std::cout << ',';
				std::cout << fieldValue(row, stmt.columns[i]);
			}
			std::cout << '\n';
		}
		++count;
	});
	std::cout << "Total rows: " << count << '\n';
}
