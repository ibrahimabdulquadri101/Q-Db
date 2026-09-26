#ifndef TABLE_H
#define TABLE_H
#include <string>
#include <vector>
#include "ColumnDef.h"
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
        std::vector<ColumnDef> columnDefs;
    public:
        Table();
        void init(std::string name , DiskManager* disk);
        void restorePageIDs(const std::vector<PageID>& pageIDs);
        void setName(std::string name);
        void setDisk(DiskManager* disk);
        void setOwner(Catalog* catalog);
        void setColumnDefs(const std::vector<ColumnDef>& cols);
        const std::vector<ColumnDef>& getColumnDefs() const;
        bool insert(const Row& row, PageID* insertedPageID = nullptr,
                uint16_t* insertedSlotID = nullptr);
        bool erase(PageID pageID, uint16_t slotID);
        bool update(PageID pageID, uint16_t slotID, const Row& row);
        void scan(std::function<void(const Row&)> callback);
        void scanLocated(std::function<void(const Row&, PageID, uint16_t)> callback);
        std::string getName();
        std::vector<PageID> getPageIDs() const;
};

#endif // TABLE_H
