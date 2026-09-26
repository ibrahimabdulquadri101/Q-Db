#include "Catalog.h"
#include <iostream>

void Catalog::init(DiskManager* disk)
{
    this->disk = disk;
    this->tables.clear();

    if (this->disk != nullptr)
    {
        std::unordered_map<std::string, std::vector<PageID>> savedTables;
        this->disk->loadCatalog(savedTables);

        for (const auto& [tableName, pageIDs] : savedTables)
        {
            auto [it, inserted] = this->tables.emplace(tableName, Table{});
            if (inserted)
            {
                it->second.setName(tableName);
                it->second.setDisk(this->disk);
                it->second.setOwner(this);
                it->second.restorePageIDs(pageIDs);
            }
        }
    }
}

Table* Catalog::createTable(std::string name)
{
    if (this->disk == nullptr)
    {
        return nullptr;
    }

    auto [it, inserted] = this->tables.emplace(name, Table{});
    if (!inserted)
    {
        return &it->second;
    }

    it->second.setName(name);
    it->second.setDisk(this->disk);
    it->second.setOwner(this);
    it->second.init(name, this->disk);
    this->save();
    return &it->second;
}

Table* Catalog::getTable(std::string name)
{
    if (this->disk == nullptr)
    {
        return nullptr;
    }

    auto it = this->tables.find(name);
    if (it == this->tables.end())
    {
        return nullptr;
    }

    return &it->second;
}

bool Catalog::tableExists(std::string name)
{
    if (this->disk == nullptr)
    {
        return false;
    }

    return this->tables.find(name) != this->tables.end();
}

void Catalog::save()
{
    if (this->disk == nullptr)
    {
        return;
    }

    std::unordered_map<std::string, std::vector<PageID>> snapshot;
    for (const auto& [tableName, table] : this->tables)
    {
        snapshot.emplace(tableName, table.getPageIDs());
    }

    this->disk->saveCatalog(snapshot);
}

bool Catalog::dropTable(std::string name)
{
    if (this->disk == nullptr)
    {
        return false;
    }
    auto it = this->tables.find(name);
    if (it == this->tables.end())
    {
        return false;
    }
    this->tables.erase(it);
    this->save();
    return true;
}