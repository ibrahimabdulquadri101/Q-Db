#ifndef DISKMANAGER_H
#define DISKMANAGER_H
#include "Page.h"
#include <fstream>
#include <string>

class DiskManager
{
    private:
        std::fstream file;
        std::string filepath;
        PageID totalPages;
        PageID freeListHead;
    public:
        bool open(std::string path);
        bool readPage(PageID id , Page& page);
        bool writePage(PageID id , const Page& page);
        PageID allocatePage();
        void freePage(PageID id);
        void close();
};

#endif // DISKMANAGER_H
