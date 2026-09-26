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
#include <utility>
#include <vector>
#include <filesystem>

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

std::string indexKey(const std::string &tableName, const std::string &column)
{
	return tableName + "__" + uppercase(column);
}

bool matches(const Row &row, const WhereClause &where)
{
	const std::string column = uppercase(where.column);
	const std::string &op = where.op;
	if (op != "=" && op != "<" && op != ">" && op != "<=" && op != ">=" && op != "!=")
	{
		throw std::invalid_argument("Unsupported comparison operator: " + op);
	}

	if (column == "ID" || column == "AGE")
	{
		const int32_t actual = column == "ID" ? row.id : row.age;
		const int32_t expected = parseInteger(where.value);
		if (op == "=")  return actual == expected;
		if (op == "!=") return actual != expected;
		if (op == "<")  return actual <  expected;
		if (op == "<=") return actual <= expected;
		if (op == ">")  return actual >  expected;
		return actual >= expected;
	}

	if (column == "NAME")
	{
		const std::string actual(row.name);
		if (op == "=")  return actual == where.value;
		if (op == "!=") return actual != where.value;
		if (op == "<")  return actual <  where.value;
		if (op == "<=") return actual <= where.value;
		if (op == ">")  return actual >  where.value;
		return actual >= where.value;
	}

	throw std::invalid_argument("Unknown column: " + where.column);
}
}

Executor::Executor(Catalog *catalog, DiskManager *disk) : catalog(catalog), disk(disk) {}

Executor::~Executor()
{
	if (disk == nullptr)
	{
		return;
	}
	for (auto &[key, index] : indexes)
	{
		try
		{
			index.saveToPages(*disk);
		}
		catch (const std::exception &error)
		{
			std::cerr << "Failed to save index " << key << ": " << error.what() << '\n';
		}
	}
}

HashIndex &Executor::getOrBuildIndex(const std::string &tableName, const std::string &column)
{
	const std::string normalizedColumn = uppercase(column);
	if (normalizedColumn != "ID" && normalizedColumn != "NAME" && normalizedColumn != "AGE")
	{
		throw std::invalid_argument("Unknown column: " + column);
	}
	const std::string key = indexKey(tableName, normalizedColumn);
	auto it = indexes.find(key);
	if (it == indexes.end())
	{
		it = indexes.emplace(key, HashIndex{}).first;
		it->second.init(key);
		it->second.loadFromPages(*disk);
	}
	if (it->second.empty())
	{
		buildIndex(tableName, normalizedColumn);
	}
	return indexes.at(key);
}

BTree &Executor::getOrBuildBTree(const std::string &tableName, const std::string &column)
{
	const std::string normalizedColumn = uppercase(column);
	if (normalizedColumn != "ID" && normalizedColumn != "AGE")
	{
		throw std::invalid_argument("B-tree indexes require an integer column: " + column);
	}
	const std::string key = indexKey(tableName, normalizedColumn);
	auto it = btrees.find(key);
	if (it == btrees.end())
	{
		it = btrees.emplace(key, BTree{}).first;
		it->second.init(disk, key);
	}
	if (it->second.empty())
	{
		buildBTree(tableName, normalizedColumn);
	}
	return btrees.at(key);
}

void Executor::buildIndex(std::string tableName, std::string column)
{
	if (catalog == nullptr || disk == nullptr)
	{
		throw std::runtime_error("Executor is not initialized");
	}
	const std::string normalizedColumn = uppercase(column);
	Table *table = catalog->getTable(tableName);
	if (table == nullptr)
	{
		throw std::invalid_argument("Table not found: " + tableName);
	}
	const std::string key = indexKey(tableName, normalizedColumn);
	HashIndex &index = indexes[key];
	index.init(key);
	table->scanLocated([&](const Row &row, PageID pageID, uint16_t slotID) {
		index.insert(fieldValue(row, normalizedColumn), pageID, slotID);
	});
	index.saveToPages(*disk);
}

void Executor::buildBTree(std::string tableName, std::string column)
{
	if (catalog == nullptr || disk == nullptr)
	{
		throw std::runtime_error("Executor is not initialized");
	}
	const std::string normalizedColumn = uppercase(column);
	if (normalizedColumn != "ID" && normalizedColumn != "AGE")
	{
		throw std::invalid_argument("B-tree indexes require an integer column: " + column);
	}
	Table *table = catalog->getTable(tableName);
	if (table == nullptr)
	{
		throw std::invalid_argument("Table not found: " + tableName);
	}
	const std::string key = indexKey(tableName, normalizedColumn);
	auto it = btrees.find(key);
	if (it == btrees.end())
	{
		it = btrees.emplace(key, BTree{}).first;
		it->second.init(disk, key);
	}
	BTree &tree = it->second;
	if (!tree.empty())
	{
		return;
	}
	table->scanLocated([&](const Row &row, PageID pageID, uint16_t slotID) {
		const int32_t value = normalizedColumn == "ID" ? row.id : row.age;
		tree.insert(value, pageID, slotID);
	});
}

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
		else if (command == "CREATE")
		{
			CreateTableStmt statement = parseCreateTable(tokens);
			executeCreateTable(statement);
		}
		else if (command == "DELETE")
		{
			DeleteStmt statement = parseDelete(tokens);
			executeDelete(statement);
		}
		else if (command == "UPDATE")
		{
			UpdateStmt statement = parseUpdate(tokens);
			executeUpdate(statement);
		}
		else if (command == "DROP")
		{
			DropTableStmt statement = parseDropTable(tokens);
			executeDropTable(statement);
		}
		else
		{
			std::cout << "Unknown command\n";
		}
	}
	catch (const std::exception &error)
	{
		std::cerr << "Error: " << error.what() << '\n';
	}
}

int32_t Executor::getNextId(const std::string &tableName)
{
	if (catalog == nullptr)
	{
		return 1;
	}
	Table *table = catalog->getTable(tableName);
	if (table == nullptr)
	{
		return 1;
	}
	// Collect all existing IDs then find the first gap starting at 1
	std::vector<int32_t> usedIds;
	table->scan([&](const Row &row) {
		if (row.id > 0)
		{
			usedIds.push_back(row.id);
		}
	});
	std::sort(usedIds.begin(), usedIds.end());
	usedIds.erase(std::unique(usedIds.begin(), usedIds.end()), usedIds.end());
	int32_t next = 1;
	for (int32_t id : usedIds)
	{
		if (id == next)
		{
			++next;
		}
		else
		{
			break;
		}
	}
	return next;
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

	Row row{};
	if (stmt.values.size() == 2)
	{
		// Auto-generate ID: INSERT INTO table VALUES ('name', age)
		row.id = getNextId(stmt.table);
		if (stmt.values[0].size() >= sizeof(Row::name))
		{
			throw std::invalid_argument("Name must be shorter than 32 characters");
		}
		std::memcpy(row.name, stmt.values[0].data(), stmt.values[0].size());
		row.age = parseInteger(stmt.values[1]);
	}
	else if (stmt.values.size() == 3)
	{
		int32_t parsedId = parseInteger(stmt.values[0]);
		if (parsedId <= 0)
		{
			row.id = getNextId(stmt.table);
		}
		else
		{
			row.id = parsedId;
			// Conflict check: ensure ID is unique
			bool conflict = false;
			table->scan([&](const Row &existing) {
				if (existing.id == row.id)
				{
					conflict = true;
				}
			});
			if (conflict)
			{
				throw std::invalid_argument("ID conflict: row with id " + std::to_string(row.id) + " already exists in table '" + stmt.table + "'");
			}
		}

		if (stmt.values[1].size() >= sizeof(Row::name))
		{
			throw std::invalid_argument("Name must be shorter than 32 characters");
		}
		std::memcpy(row.name, stmt.values[1].data(), stmt.values[1].size());
		row.age = parseInteger(stmt.values[2]);
	}
	else
	{
		throw std::invalid_argument("INSERT requires 2 values (name, age) or 3 values (id, name, age)");
	}

	BTree &idTree = getOrBuildBTree(stmt.table, "ID");
	BTree &ageTree = getOrBuildBTree(stmt.table, "AGE");
	PageID pageID = 0;
	uint16_t slotID = 0;
	if (!table->insert(row, &pageID, &slotID))
	{
		throw std::runtime_error("Failed to insert row into table: " + stmt.table);
	}
	idTree.insert(row.id, pageID, slotID);
	ageTree.insert(row.age, pageID, slotID);
	const std::string tablePrefix = stmt.table + "__";
	for (auto &[key, index] : indexes)
	{
		if (key.compare(0, tablePrefix.size(), tablePrefix) == 0)
		{
			const std::string column = key.substr(tablePrefix.size());
			index.insert(fieldValue(row, column), pageID, slotID);
		}
	}
	std::cout << "Inserted 1 row (id = " << row.id << ")\n";
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

	// Determine which columns to display
	std::vector<std::string> headers;
	if (stmt.columns.empty())
	{
		headers = {"id", "name", "age"};
	}
	else
	{
		for (const std::string &col : stmt.columns)
		{
			const std::string normalized = uppercase(col);
			if (normalized != "ID" && normalized != "NAME" && normalized != "AGE")
			{
				throw std::invalid_argument("Unknown column: " + col);
			}
			headers.push_back(col);
		}
	}
	if (stmt.hasWhere)
	{
		matches(Row{}, stmt.where); // validate where clause eagerly
	}

	// ----------------------------------------------------------------
	// Collect all matching rows into memory so we can compute widths
	// ----------------------------------------------------------------
	using ResultRow = std::vector<std::string>;
	std::vector<ResultRow> results;

	auto collectRow = [&](const Row &row) {
		if (stmt.hasWhere && !matches(row, stmt.where))
		{
			return;
		}
		ResultRow cells;
		cells.reserve(headers.size());
		for (const std::string &col : headers)
		{
			cells.push_back(fieldValue(row, col));
		}
		results.push_back(std::move(cells));
	};

	// ---- query execution ----
	const bool integerColumn = stmt.hasWhere &&
		(uppercase(stmt.where.column) == "ID" || uppercase(stmt.where.column) == "AGE");
	const std::string &whereOp = stmt.hasWhere ? stmt.where.op : std::string{};
	// BTree can accelerate: = (point lookup), > (range from), >= (range from), </<=/!= (full scan with filter)
	const bool canUseBTree = integerColumn &&
		(whereOp == "=" || whereOp == ">" || whereOp == ">=");
	if (canUseBTree)
	{
		const int32_t value = parseInteger(stmt.where.value);
		BTree &tree = getOrBuildBTree(stmt.table, stmt.where.column);
		if (whereOp == "=" && uppercase(stmt.where.column) == "ID")
		{
			// Point lookup with fallback scan
			bool found = false;
			PageID pageID = 0;
			uint16_t slotID = 0;
			if (tree.search(value, pageID, slotID))
			{
				Page page{};
				Row row{};
				if (disk->readPage(pageID, page) && readRow(page, slotID, row) && row.id == value)
				{
					collectRow(row);
					found = true;
				}
			}
			if (!found)
			{
				table->scan([&](const Row &row) {
					if (row.id == value) collectRow(row);
				});
			}
		}
		else if (whereOp == ">")
		{
			if (value < std::numeric_limits<int32_t>::max())
			{
				std::vector<std::pair<PageID, uint16_t>> locations;
				tree.rangeSearch(value + 1, [&](PageID pageID, uint16_t slotID) {
					locations.push_back({pageID, slotID});
				});
				std::sort(locations.begin(), locations.end());
				Page cachedPage{};
				PageID cachedPageID = 0;
				for (const auto &location : locations)
				{
					if (location.first != cachedPageID)
					{
						if (!disk->readPage(location.first, cachedPage))
						{
							cachedPageID = 0;
							continue;
						}
						cachedPageID = location.first;
					}
					Row row{};
					if (readRow(cachedPage, location.second, row))
					{
						collectRow(row);
					}
				}
			}
		}
		else if (whereOp == ">=")
		{
			std::vector<std::pair<PageID, uint16_t>> locations;
			tree.rangeSearch(value, [&](PageID pageID, uint16_t slotID) {
				locations.push_back({pageID, slotID});
			});
			std::sort(locations.begin(), locations.end());
			Page cachedPage{};
			PageID cachedPageID = 0;
			for (const auto &location : locations)
			{
				if (location.first != cachedPageID)
				{
					if (!disk->readPage(location.first, cachedPage))
					{
						cachedPageID = 0;
						continue;
					}
					cachedPageID = location.first;
				}
				Row row{};
				if (readRow(cachedPage, location.second, row))
				{
					collectRow(row);
				}
			}
		}
		else
		{
			// =, age equality, etc. — rangeSearch by value with exact match
			std::vector<std::pair<PageID, uint16_t>> locations;
			tree.rangeSearch(value, [&](PageID pageID, uint16_t slotID) {
				locations.push_back({pageID, slotID});
			});
			std::sort(locations.begin(), locations.end());
			Page cachedPage{};
			PageID cachedPageID = 0;
			for (const auto &location : locations)
			{
				if (location.first != cachedPageID)
				{
					if (!disk->readPage(location.first, cachedPage))
					{
						cachedPageID = 0;
						continue;
					}
					cachedPageID = location.first;
				}
				Row row{};
				if (readRow(cachedPage, location.second, row))
				{
					collectRow(row);
				}
			}
		}
	}
	else
	{
		const bool canUseIndex = stmt.hasWhere && stmt.where.op == "=" &&
			(uppercase(stmt.where.column) == "ID" || uppercase(stmt.where.column) == "NAME" ||
			 uppercase(stmt.where.column) == "AGE");
		if (canUseIndex)
		{
			bool found = false;
			HashIndex &index = getOrBuildIndex(stmt.table, stmt.where.column);
			PageID pageID = 0;
			uint16_t slotID = 0;
			std::string searchValue = stmt.where.value;
			const std::string indexedColumn = uppercase(stmt.where.column);
			if (indexedColumn == "ID" || indexedColumn == "AGE")
			{
				searchValue = std::to_string(parseInteger(searchValue));
			}
			if (index.lookup(searchValue, pageID, slotID))
			{
				Page page{};
				Row row{};
				if (disk->readPage(pageID, page) && readRow(page, slotID, row))
				{
					collectRow(row);
					found = true;
				}
			}
			if (!found)
			{
				table->scan(collectRow);
			}
		}
		else
		{
			table->scan(collectRow);
		}
	}

	// ----------------------------------------------------------------
	// Sort results by ID ascending if ID is displayed, for clean ordered tables
	// ----------------------------------------------------------------
	std::size_t idColIndex = std::string::npos;
	for (std::size_t c = 0; c < headers.size(); ++c)
	{
		if (uppercase(headers[c]) == "ID")
		{
			idColIndex = c;
			break;
		}
	}
	if (idColIndex != std::string::npos)
	{
		std::sort(results.begin(), results.end(), [idColIndex](const ResultRow &a, const ResultRow &b) {
			try {
				return std::stoll(a[idColIndex]) < std::stoll(b[idColIndex]);
			} catch (...) {
				return a[idColIndex] < b[idColIndex];
			}
		});
	}

	// ----------------------------------------------------------------
	// Compute column widths  (max of header width and widest cell)
	// ----------------------------------------------------------------
	std::vector<std::size_t> widths(headers.size());
	for (std::size_t c = 0; c < headers.size(); ++c)
	{
		widths[c] = headers[c].size();
	}
	for (const ResultRow &row : results)
	{
		for (std::size_t c = 0; c < row.size(); ++c)
		{
			if (row[c].size() > widths[c])
			{
				widths[c] = row[c].size();
			}
		}
	}

	// ----------------------------------------------------------------
	// Print table
	// ----------------------------------------------------------------
	// Helper: print a horizontal divider  +-------+------+-----+
	auto printDivider = [&]() {
		std::cout << '+';
		for (std::size_t c = 0; c < widths.size(); ++c)
		{
			std::cout << std::string(widths[c] + 2, '-') << '+';
		}
		std::cout << '\n';
	};

	// Helper: print one row of cells  | val  | val  | val |
	auto printRow = [&](const ResultRow &cells) {
		std::cout << '|';
		for (std::size_t c = 0; c < cells.size(); ++c)
		{
			std::cout << ' ' << cells[c]
			          << std::string(widths[c] - cells[c].size(), ' ')
			          << " |";
		}
		std::cout << '\n';
	};

	printDivider();
	printRow(headers);
	printDivider();
	for (const ResultRow &row : results)
	{
		printRow(row);
	}
	printDivider();
	std::cout << results.size() << " row(s)\n";
}

void Executor::executeCreateTable(CreateTableStmt &stmt)
{
	if (catalog == nullptr || disk == nullptr)
	{
		throw std::runtime_error("Executor is not initialized");
	}
	if (catalog->tableExists(stmt.table))
	{
		std::cout << "Table '" << stmt.table << "' already exists\n";
		return;
	}
	Table *table = catalog->createTable(stmt.table);
	if (table == nullptr)
	{
		throw std::runtime_error("Failed to create table: " + stmt.table);
	}
	table->setColumnDefs(stmt.columns);
	std::cout << "Created table " << stmt.table << "\n";
}

void Executor::executeDelete(DeleteStmt &stmt)
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
	if (stmt.hasWhere)
	{
		matches(Row{}, stmt.where);
	}

	struct DeleteMatch
	{
		PageID pageID;
		uint16_t slotID;
		Row row;
	};
	std::vector<DeleteMatch> toDelete;

	table->scanLocated([&](const Row &row, PageID pageID, uint16_t slotID) {
		if (!stmt.hasWhere || matches(row, stmt.where))
		{
			toDelete.push_back({pageID, slotID, row});
		}
	});

	std::size_t deletedCount = 0;
	const std::string tablePrefix = stmt.table + "__";
	for (const auto &match : toDelete)
	{
		if (table->erase(match.pageID, match.slotID))
		{
			++deletedCount;
			for (auto &[key, index] : indexes)
			{
				if (key.compare(0, tablePrefix.size(), tablePrefix) == 0)
				{
					const std::string column = key.substr(tablePrefix.size());
					index.remove(fieldValue(match.row, column));
				}
			}
		}
	}
	std::cout << "Deleted " << deletedCount << " row(s)\n";
}

void Executor::executeUpdate(UpdateStmt &stmt)
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

	for (const auto &assign : stmt.assignments)
	{
		const std::string col = uppercase(assign.column);
		if (col != "ID" && col != "NAME" && col != "AGE")
		{
			throw std::invalid_argument("Unknown column in SET: " + assign.column);
		}
		if (col == "ID" || col == "AGE")
		{
			parseInteger(assign.value);
		}
		else if (col == "NAME")
		{
			if (assign.value.size() >= sizeof(Row::name))
			{
				throw std::invalid_argument("Name must be shorter than 32 characters");
			}
		}
	}

	if (stmt.hasWhere)
	{
		matches(Row{}, stmt.where);
	}

	struct UpdateMatch
	{
		PageID pageID;
		uint16_t slotID;
		Row oldRow;
		Row newRow;
	};
	std::vector<UpdateMatch> toUpdate;

	table->scanLocated([&](const Row &row, PageID pageID, uint16_t slotID) {
		if (!stmt.hasWhere || matches(row, stmt.where))
		{
			Row updated = row;
			for (const auto &assign : stmt.assignments)
			{
				const std::string col = uppercase(assign.column);
				if (col == "ID")
				{
					int32_t newId = parseInteger(assign.value);
					if (newId <= 0)
					{
						throw std::invalid_argument("ID must be a positive integer");
					}
					if (newId != row.id)
					{
						bool conflict = false;
						table->scan([&](const Row &existing) {
							if (existing.id == newId)
							{
								conflict = true;
							}
						});
						if (conflict)
						{
							throw std::invalid_argument("ID conflict: row with id " + std::to_string(newId) + " already exists in table '" + stmt.table + "'");
						}
					}
					updated.id = newId;
				}
				else if (col == "NAME")
				{
					std::memset(updated.name, 0, sizeof(updated.name));
					std::memcpy(updated.name, assign.value.data(), assign.value.size());
				}
				else if (col == "AGE")
				{
					updated.age = parseInteger(assign.value);
				}
			}
			toUpdate.push_back({pageID, slotID, row, updated});
		}
	});

	std::size_t updatedCount = 0;
	const std::string tablePrefix = stmt.table + "__";
	for (const auto &match : toUpdate)
	{
		if (table->update(match.pageID, match.slotID, match.newRow))
		{
			++updatedCount;
			for (auto &[key, index] : indexes)
			{
				if (key.compare(0, tablePrefix.size(), tablePrefix) == 0)
				{
					const std::string column = key.substr(tablePrefix.size());
					index.remove(fieldValue(match.oldRow, column));
					index.insert(fieldValue(match.newRow, column), match.pageID, match.slotID);
				}
			}

			auto idIt = btrees.find(indexKey(stmt.table, "ID"));
			if (idIt != btrees.end() && match.oldRow.id != match.newRow.id)
			{
				idIt->second.insert(match.newRow.id, match.pageID, match.slotID);
			}
			auto ageIt = btrees.find(indexKey(stmt.table, "AGE"));
			if (ageIt != btrees.end() && match.oldRow.age != match.newRow.age)
			{
				ageIt->second.insert(match.newRow.age, match.pageID, match.slotID);
			}
		}
	}
	std::cout << "Updated " << updatedCount << " row(s)\n";
}

void Executor::executeDropTable(DropTableStmt &stmt)
{
	if (catalog == nullptr || disk == nullptr)
	{
		throw std::runtime_error("Executor is not initialized");
	}
	if (!catalog->tableExists(stmt.table))
	{
		if (stmt.ifExists)
		{
			std::cout << "Table '" << stmt.table << "' does not exist (skipped)\n";
			return;
		}
		throw std::invalid_argument("Table not found: " + stmt.table);
	}

	// Drop all in-memory indexes and B-trees for this table
	const std::string tablePrefix = stmt.table + "__";
	for (auto it = indexes.begin(); it != indexes.end(); )
	{
		if (it->first.compare(0, tablePrefix.size(), tablePrefix) == 0)
		{
			it = indexes.erase(it);
		}
		else
		{
			++it;
		}
	}
	for (auto it = btrees.begin(); it != btrees.end(); )
	{
		if (it->first.compare(0, tablePrefix.size(), tablePrefix) == 0)
		{
			it = btrees.erase(it);
		}
		else
		{
			++it;
		}
	}

	// Delete physical .btree files from disk (even if not loaded in memory)
	std::filesystem::path dbPath(disk->getFilePath());
	if (dbPath.has_parent_path())
	{
		std::string prefix = dbPath.filename().string() + "." + tablePrefix;
		for (const auto& entry : std::filesystem::directory_iterator(dbPath.parent_path()))
		{
			if (entry.is_regular_file())
			{
				std::string filename = entry.path().filename().string();
				if (filename.compare(0, prefix.size(), prefix) == 0 &&
					filename.size() >= 6 && filename.substr(filename.size() - 6) == ".btree")
				{
					std::filesystem::remove(entry.path());
				}
			}
		}
	}

	catalog->dropTable(stmt.table);
	std::cout << "Dropped table " << stmt.table << "\n";
}
