#include "Table.h"
#include "Catalog.h"
#include "DiskManager.h"
#include "Page.h"
#include <utility>
#include <iostream>

Table::Table() : name(), PageIDs(), disk(nullptr), owner(nullptr) {}

void Table::init(std::string name, DiskManager* disk)
{
    this->name = std::move(name);
    this->disk = disk;
    this->PageIDs.clear();

    if (this->disk != nullptr)
    {
        this->PageIDs.push_back(this->disk->allocatePage());
    }
}

void Table::restorePageIDs(const std::vector<PageID>& pageIDs)
{
    this->PageIDs = pageIDs;
}

void Table::setName(std::string name)
{
    this->name = std::move(name);
}

void Table::setDisk(DiskManager* disk)
{
    this->disk = disk;
}

void Table::setOwner(Catalog* catalog)
{
    this->owner = catalog;
}

bool Table::insert(const Row& row)
{
    if (this->disk == nullptr)
    {
        return false;
    }

    for (PageID pageID : this->PageIDs)
    {
        Page page{};
        if (!this->disk->readPage(pageID, page))
        {
            return false;
        }

        if (hasSpace(page))
        {
            uint16_t slotID = 0;
            if (!insertRow(page, row, slotID))
            {
                return false;
            }
            bool ok = this->disk->writePage(pageID, page);
            return ok;
        }
    }

    PageID newPageID = this->disk->allocatePage();
    Page page{};
    page.id = newPageID;
    this->PageIDs.push_back(newPageID);

    uint16_t slotID = 0;
    if (!insertRow(page, row, slotID))
    {
        return false;
    }

    bool ok = this->disk->writePage(newPageID, page);
    if (ok && this->owner != nullptr)
    {
        this->owner->save();
    }
    return ok;
}

void Table::scan(std::function<void(const Row&)> callback)
{
    if (this->disk == nullptr || callback == nullptr)
    {
        return;
    }

    for (PageID pageId : this->PageIDs)
    {
        Page page{};
        if (!this->disk->readPage(pageId, page))
        {
            continue;
        }

        const uint16_t slotCount = getSlotCount(page);
        for (uint16_t slotID = 0; slotID < slotCount; ++slotID)
        {
            Row row{};
            if (readRow(page, slotID, row))
            {
                callback(row);
            }
        }
    }
}

std::string Table::getName()
{
    return this->name;
}

std::vector<PageID> Table::getPageIDs() const
{
    return this->PageIDs;
}

