#ifndef WAL_H
#define WAL_H

#include "Page.h"
#include "DiskManager.h"
#include <string>
#include <fstream>
#include <cstdint>

enum class WALOp { INSERT, DELETE, UPDATE };

struct WALRecord {
    WALOp op;
    PageID pageID;
    uint16_t slotID;
    uint8_t before[ROW_SIZE];   // what was there before
    uint8_t after[ROW_SIZE];    // what will be there after
    uint32_t checksum;          // detect partial writes
};

class WAL {
public:
    void init(std::string logPath);
    void logInsert(PageID pageID, uint16_t slotID, const Row& row);
    void logDelete(PageID pageID, uint16_t slotID, const Row& row);
    void logUpdate(PageID pageID, uint16_t slotID, const Row& oldRow, const Row& newRow);
    void replay(DiskManager& disk);
    void checkpoint();
    
private:
    std::fstream logFile;
    std::string path;
    uint32_t calculateChecksum(const WALRecord& record);
};

#endif // WAL_H
