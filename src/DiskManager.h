#ifndef DISKMANAGER_H
#define DISKMANAGER_H
#include "Page.h"
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

class WAL;

class DiskManager
{
    private:
        std::fstream file;
        std::string filepath;
        PageID totalPages;
        PageID freeListHead;
        WAL* wal = nullptr;
    public:
        bool open(std::string path);
        bool readPage(PageID id , Page& page);
        bool writePage(PageID id , const Page& page);
        PageID allocatePage();
        void freePage(PageID id);
        bool saveCatalog(const std::unordered_map<std::string, std::vector<PageID>>& tables);
        bool loadCatalog(std::unordered_map<std::string, std::vector<PageID>>& tables);
        void close();
        bool sync();
        const std::string& getFilePath() const;
        void setWAL(WAL* wal);
        WAL* getWAL() const;
};

#endif // DISKMANAGER_H
