#ifndef CATALOG_H
#define CATALOG_H
#include "Table.h"
#include <unordered_map>
#include <string>

class Catalog
{
    private:
        std::unordered_map<std::string,Table> tables;
        DiskManager* disk;
    public:
        void init(DiskManager* disk);
        void save();
        Table* createTable(std::string name);
        Table* getTable(std::string name);
        bool tableExists(std::string name);
};

#endif // CATALOG_H
