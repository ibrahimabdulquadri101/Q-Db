#ifndef COLUMNDEF_H
#define COLUMNDEF_H

#include <string>

struct ColumnDef
{
	std::string name;
	std::string type; // "INT" or "CHAR"
};

#endif // COLUMNDEF_H
