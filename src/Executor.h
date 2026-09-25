#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "Parser.h"

class Catalog;
class DiskManager;

class Executor
{
private:
	Catalog *catalog;
	DiskManager *disk;

public:
	Executor(Catalog *catalog, DiskManager *disk);
	void execute(std::string sql);
	void executeInsert(InsertStmt &stmt);
	void executeSelect(SelectStmt &stmt);
};

#endif // EXECUTOR_H
