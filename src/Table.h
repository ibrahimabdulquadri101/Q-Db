#ifndef TABLE_H
#define TABLE_H
#include <string>
#include <vector>
#include "Page.h"
#include "DiskManager.h"
#include <functional>
class Table
{
    private:
        std::string name;
        std::vector<PageID> PageIDs;
        DiskManager* disk;
    public:
        void init(std::string name , DiskManager* disk);
        bool insert(const Row& row);
        void scan(std::function<void(const Row&)> callback);
        std::string getName();
        std::vector<PageID> getPageIDs();
};

#endif // TABLE_H
