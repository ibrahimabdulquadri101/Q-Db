#include <iostream>
#include <fstream>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include "Page.h"
#include "DiskManager.h"

bool DiskManager::open(std::string path)
{
    filepath = path;
    file.open(path, std::ios::in | std::ios::out | std::ios::binary);

    if (!file.is_open())
    {
        std::cout << " File not found , Creating it" << std::endl;
        file.clear();
        file.open(path, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
        this->totalPages = 1;
        this->freeListHead = 0;

        file.seekp(0, std::ios::beg);
        file.write(reinterpret_cast<const char*>(&this->totalPages), sizeof(PageID));
        file.write(reinterpret_cast<const char*>(&this->freeListHead), sizeof(PageID));

        file.seekp(8, std::ios::beg);
        uint32_t zeroCount = 0;
        file.write(reinterpret_cast<const char*>(&zeroCount), sizeof(zeroCount));
    }
    else
    {
        std::cout << "Opened file sucessfully \n";
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(&totalPages), sizeof(PageID));
        file.read(reinterpret_cast<char*>(&freeListHead), sizeof(PageID));
    }

    if (!file.is_open())
    {
        std::cerr << "Critical error: Could not open or create file.\n";
        return false;
    }
    return true;
}

bool DiskManager::readPage(PageID id , Page& page)
{
    auto offset = id * PAGE_SIZE;
    file.seekg(offset,std::ios::beg);
    file.read(reinterpret_cast<char*>(&page.data),PAGE_SIZE);
    if(file.fail())
    {
        std::cerr << "Error" ;
        file.clear();
        return false;
    }
    page.id = id;
    return true;
}

bool DiskManager::writePage(PageID id , const Page& page)
{
    auto offset = id * PAGE_SIZE;
    file.seekp(offset,std::ios::beg);
    file.write(reinterpret_cast<const char*>(&page.data),PAGE_SIZE);
    if(file.fail())
    {
        std::cerr << "Error" ;
        file.clear();
        return false;
    }
    file.flush();
    return true;
}

PageID DiskManager::allocatePage()
{
    PageID allocatedId;

    if (freeListHead != 0)
    {
        allocatedId = freeListHead;

        auto offset = allocatedId * PAGE_SIZE;
        file.seekg(offset, std::ios::beg);

        PageID nextFree;
        file.read(reinterpret_cast<char*>(&nextFree), sizeof(PageID));

        freeListHead = nextFree;
    }
    else
    {
        allocatedId = totalPages;
        totalPages++;
    }

    file.seekp((allocatedId + 1) * PAGE_SIZE - 1, std::ios::beg);
    char zero = 0;
    file.write(&zero, 1);

    file.seekp(0, std::ios::beg);
    file.write(reinterpret_cast<const char*>(&totalPages), sizeof(PageID));
    file.write(reinterpret_cast<const char*>(&freeListHead), sizeof(PageID));
    file.flush();

    return allocatedId;
}

bool DiskManager::saveCatalog(const std::unordered_map<std::string, std::vector<PageID>>& tables)
{
    if (!file.is_open() || filepath.empty())
    {
        return false;
    }

    const std::string path = filepath + ".catalog";
    const std::string temporaryPath = path + ".tmp";
    std::ofstream catalogFile(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!catalogFile)
    {
        return false;
    }
    const uint32_t tableCount = static_cast<uint32_t>(tables.size());
    catalogFile.write(reinterpret_cast<const char*>(&tableCount), sizeof(tableCount));

    for (const auto& [name, pageIDs] : tables)
    {
        uint32_t nameLength = static_cast<uint32_t>(name.size());
        uint32_t pageCount = static_cast<uint32_t>(pageIDs.size());
        catalogFile.write(reinterpret_cast<const char*>(&nameLength), sizeof(nameLength));
        catalogFile.write(name.data(), nameLength);
        catalogFile.write(reinterpret_cast<const char*>(&pageCount), sizeof(pageCount));
        for (PageID pageID : pageIDs)
        {
            catalogFile.write(reinterpret_cast<const char*>(&pageID), sizeof(pageID));
        }
    }

    catalogFile.close();
    if (!catalogFile)
    {
        return false;
    }
    std::remove(path.c_str());
    return std::rename(temporaryPath.c_str(), path.c_str()) == 0;
}

bool DiskManager::loadCatalog(std::unordered_map<std::string, std::vector<PageID>>& tables)
{
    tables.clear();
    if (!file.is_open() || filepath.empty())
    {
        return false;
    }

    std::ifstream catalogFile(filepath + ".catalog", std::ios::binary);
    if (!catalogFile)
    {
        return true;
    }
    uint32_t tableCount = 0;
    catalogFile.read(reinterpret_cast<char*>(&tableCount), sizeof(tableCount));
    if (!catalogFile || tableCount > 1000000)
    {
        return false;
    }

    for (uint32_t i = 0; i < tableCount; ++i)
    {
        uint32_t nameLength = 0;
        catalogFile.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
        if (!catalogFile || nameLength == 0 || nameLength > 1024 * 1024)
        {
            return false;
        }

        std::string name(nameLength, '\0');
        catalogFile.read(&name[0], nameLength);
        if (!catalogFile)
        {
            return false;
        }

        uint32_t pageCount = 0;
        catalogFile.read(reinterpret_cast<char*>(&pageCount), sizeof(pageCount));
        if (!catalogFile || pageCount > 100000000)
        {
            return false;
        }

        std::vector<PageID> pageIDs(pageCount);
        for (uint32_t j = 0; j < pageCount; ++j)
        {
            catalogFile.read(reinterpret_cast<char*>(&pageIDs[j]), sizeof(PageID));
            if (!catalogFile)
            {
                return false;
            }
        }

        tables.emplace(name, std::move(pageIDs));
    }

    return true;
}

void DiskManager::freePage(PageID id)
{
    auto offset = id * PAGE_SIZE;
    file.seekp(offset, std::ios::beg);
    file.write(reinterpret_cast<const char*>(&freeListHead), sizeof(PageID));

    freeListHead = id;

    file.seekp(0, std::ios::beg);
    file.write(reinterpret_cast<const char*>(&totalPages), sizeof(PageID));
    file.write(reinterpret_cast<const char*>(&freeListHead), sizeof(PageID));
    
    file.flush();
}

void DiskManager::close()
{
    if (file.is_open())
    {
        file.flush();
        file.close();
    }
}

bool DiskManager::sync()
{
    if (!file.is_open() || filepath.empty())
    {
        return false;
    }
    file.flush();
    if (!file)
    {
        file.clear();
        return false;
    }
    const int descriptor = ::open(filepath.c_str(), O_RDWR);
    if (descriptor < 0)
    {
        return false;
    }
    const int syncResult = ::fsync(descriptor);
    ::close(descriptor);
    return syncResult == 0;
}

const std::string& DiskManager::getFilePath() const
{
    return filepath;
}

void DiskManager::setWAL(WAL* walManager)
{
    wal = walManager;
}

WAL* DiskManager::getWAL() const
{
    return wal;
}