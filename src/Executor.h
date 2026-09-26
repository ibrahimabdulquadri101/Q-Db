#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "Parser.h"
#include "HashIndex.h"
#include "BTree.h"

#include <string>
#include <unordered_map>

class Catalog;
class DiskManager;

class Executor
{
private:
	Catalog *catalog;
	DiskManager *disk;
	std::unordered_map<std::string, HashIndex> indexes;
	std::unordered_map<std::string, BTree> btrees;

	HashIndex &getOrBuildIndex(const std::string &tableName, const std::string &column);
	BTree &getOrBuildBTree(const std::string &tableName, const std::string &column);

public:
	Executor(Catalog *catalog, DiskManager *disk);
	~Executor();
	void execute(std::string sql);
	void executeInsert(InsertStmt &stmt);
	void executeSelect(SelectStmt &stmt);
	void executeCreateTable(CreateTableStmt &stmt);
	void executeDelete(DeleteStmt &stmt);
	void executeUpdate(UpdateStmt &stmt);
	void executeDropTable(DropTableStmt &stmt);
	void buildIndex(std::string tableName, std::string column);
	void buildBTree(std::string tableName, std::string column);
	int32_t getNextId(const std::string &tableName);
};

#endif // EXECUTOR_H
