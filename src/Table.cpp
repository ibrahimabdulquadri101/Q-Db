#include "Table.h"
#include "Catalog.h"
#include "DiskManager.h"
#include "Page.h"
#include "Parser.h"
#include "WAL.h"
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

void Table::setColumnDefs(const std::vector<ColumnDef>& cols)
{
    this->columnDefs = cols;
}

const std::vector<ColumnDef>& Table::getColumnDefs() const
{
    return this->columnDefs;
}

bool Table::insert(const Row& row, PageID* insertedPageID, uint16_t* insertedSlotID)
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
            const uint16_t expectedSlotID = getSlotCount(page);
            if (this->disk->getWAL() != nullptr)
            {
                this->disk->getWAL()->logInsert(pageID, expectedSlotID, row);
            }
            if (!insertRow(page, row, slotID))
            {
                return false;
            }
            if (slotID != expectedSlotID)
            {
                return false;
            }
            bool ok = this->disk->writePage(pageID, page);
            if (ok && insertedPageID != nullptr)
            {
                *insertedPageID = pageID;
            }
            if (ok && insertedSlotID != nullptr)
            {
                *insertedSlotID = slotID;
            }
            return ok;
        }
    }

    PageID newPageID = this->disk->allocatePage();
    if (this->disk->getWAL() != nullptr && !this->disk->sync())
    {
        return false;
    }
    Page page{};
    page.id = newPageID;
    this->PageIDs.push_back(newPageID);
    if (this->owner != nullptr)
    {
        this->owner->save();
    }

    uint16_t slotID = 0;
    if (this->disk->getWAL() != nullptr)
    {
        this->disk->getWAL()->logInsert(newPageID, 0, row);
    }
    if (!insertRow(page, row, slotID))
    {
        return false;
    }

    bool ok = this->disk->writePage(newPageID, page);
    if (ok && insertedPageID != nullptr)
    {
        *insertedPageID = newPageID;
    }
    if (ok && insertedSlotID != nullptr)
    {
        *insertedSlotID = slotID;
    }
    return ok;
}

bool Table::erase(PageID pageID, uint16_t slotID)
{
    if (this->disk == nullptr)
    {
        return false;
    }
    Page page{};
    if (!this->disk->readPage(pageID, page))
    {
        return false;
    }
    Row row{};
    if (!readRow(page, slotID, row))
    {
        return false;
    }
    if (this->disk->getWAL() != nullptr)
    {
        this->disk->getWAL()->logDelete(pageID, slotID, row);
    }
    deleteRow(page, slotID);
    return this->disk->writePage(pageID, page);
}

bool Table::update(PageID pageID, uint16_t slotID, const Row& row)
{
    if (this->disk == nullptr)
    {
        return false;
    }
    Page page{};
    if (!this->disk->readPage(pageID, page))
    {
        return false;
    }
    Row oldRow{};
    if (!readRow(page, slotID, oldRow))
    {
        return false;
    }
    if (this->disk->getWAL() != nullptr)
    {
        this->disk->getWAL()->logUpdate(pageID, slotID, oldRow, row);
    }
    if (!updateRow(page, slotID, row))
    {
        return false;
    }
    return this->disk->writePage(pageID, page);
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

void Table::scanLocated(std::function<void(const Row&, PageID, uint16_t)> callback)
{
    if (this->disk == nullptr || callback == nullptr)
    {
        return;
    }

    for (PageID pageID : this->PageIDs)
    {
        Page page{};
        if (!this->disk->readPage(pageID, page))
        {
            continue;
        }

        const uint16_t slotCount = getSlotCount(page);
        for (uint16_t slotID = 0; slotID < slotCount; ++slotID)
        {
            Row row{};
            if (readRow(page, slotID, row))
            {
                callback(row, pageID, slotID);
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

