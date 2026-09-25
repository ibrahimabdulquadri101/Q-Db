#ifndef TABLE_H
#define TABLE_H
#include <string>
#include <vector>
#include "Page.h"
#include "DiskManager.h"
#include <functional>

class Catalog;

class Table
{
    private:
        std::string name;
        std::vector<PageID> PageIDs;
        DiskManager* disk;
        Catalog* owner;
    public:
        Table();
        void init(std::string name , DiskManager* disk);
        void restorePageIDs(const std::vector<PageID>& pageIDs);
        void setName(std::string name);
        void setDisk(DiskManager* disk);
        void setOwner(Catalog* catalog);
        bool insert(const Row& row);
        void scan(std::function<void(const Row&)> callback);
        std::string getName();
        std::vector<PageID> getPageIDs() const;
};

#endif // TABLE_H
