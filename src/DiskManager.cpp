#include <iostream>
#include <fstream>
#include "Page.h"
#include "DiskManager.h"

bool DiskManager::open(std::string path)
{
        file.open(path , std::ios::in | std::ios::out | std::ios::binary);

    if(!file.is_open())
    {
        std::cout << " File not found , Creating it" << std::endl;
        file.clear();
        file.open(path , std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
        this -> totalPages = 1;
        this -> freeListHead = 0;
        file.seekp(0, std::ios::beg);
        file.write(reinterpret_cast<const char*>(&totalPages), sizeof(PageID));
        file.write(reinterpret_cast<const char*>(&freeListHead), sizeof(PageID));
    }else
    {
        std::cout << "Opened file sucessfully \n";
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(&totalPages), sizeof(PageID));
        file.read(reinterpret_cast<char*>(&freeListHead), sizeof(PageID));
    }

    if(!file.is_open())
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

    file.seekp(0, std::ios::beg);
    file.write(reinterpret_cast<const char*>(&totalPages), sizeof(PageID));
    file.write(reinterpret_cast<const char*>(&freeListHead), sizeof(PageID));
    file.flush();

    return allocatedId;
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