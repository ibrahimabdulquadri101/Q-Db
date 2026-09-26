#include "WAL.h"
#include <cstring>

void WAL::init(std::string logPath) {
    path = logPath;
    logFile.open(path, std::ios::in | std::ios::out | std::ios::app | std::ios::binary);
    if (!logFile.is_open()) {
        logFile.clear();
        logFile.open(path, std::ios::out | std::ios::binary);
        logFile.close();
        logFile.open(path, std::ios::in | std::ios::out | std::ios::app | std::ios::binary);
    }
}

uint32_t WAL::calculateChecksum(const WALRecord& record) {
    uint32_t sum = 0;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&record);
    size_t sizeToHash = sizeof(WALRecord) - sizeof(uint32_t);
    for (size_t i = 0; i < sizeToHash; ++i) {
        sum = sum * 31 + ptr[i];
    }
    return sum;
}

void WAL::logInsert(PageID pageID, uint16_t slotID, const Row& row) {
    if (!logFile.is_open()) return;
    
    WALRecord record;
    std::memset(&record, 0, sizeof(WALRecord));
    record.op = WALOp::INSERT;
    record.pageID = pageID;
    record.slotID = slotID;
    std::memcpy(record.after, &row, ROW_SIZE);
    record.checksum = calculateChecksum(record);
    
    logFile.clear();
    logFile.seekp(0, std::ios::end);
    logFile.write(reinterpret_cast<const char*>(&record), sizeof(WALRecord));
    logFile.flush();
}

void WAL::logDelete(PageID pageID, uint16_t slotID, const Row& row) {
    if (!logFile.is_open()) return;
    
    WALRecord record;
    std::memset(&record, 0, sizeof(WALRecord));
    record.op = WALOp::DELETE;
    record.pageID = pageID;
    record.slotID = slotID;
    std::memcpy(record.before, &row, ROW_SIZE);
    record.checksum = calculateChecksum(record);
    
    logFile.clear();
    logFile.seekp(0, std::ios::end);
    logFile.write(reinterpret_cast<const char*>(&record), sizeof(WALRecord));
    logFile.flush();
}

void WAL::logUpdate(PageID pageID, uint16_t slotID, const Row& oldRow, const Row& newRow) {
    if (!logFile.is_open()) return;
    
    WALRecord record;
    std::memset(&record, 0, sizeof(WALRecord));
    record.op = WALOp::UPDATE;
    record.pageID = pageID;
    record.slotID = slotID;
    std::memcpy(record.before, &oldRow, ROW_SIZE);
    std::memcpy(record.after, &newRow, ROW_SIZE);
    record.checksum = calculateChecksum(record);
    
    logFile.clear();
    logFile.seekp(0, std::ios::end);
    logFile.write(reinterpret_cast<const char*>(&record), sizeof(WALRecord));
    logFile.flush();
}

void WAL::replay(DiskManager& disk) {
    if (!logFile.is_open()) return;
    
    logFile.clear();
    logFile.seekg(0, std::ios::beg);
    
    WALRecord record;
    while (logFile.read(reinterpret_cast<char*>(&record), sizeof(WALRecord))) {
        if (record.checksum != calculateChecksum(record)) {
            // Invalid checksum, partial write, stop
            break;
        }
        
        Page page;
        if (!disk.readPage(record.pageID, page)) {
            page.id = record.pageID;
            std::memset(page.data, 0, PAGE_SIZE);
        }
        
        if (record.op == WALOp::INSERT) {
            Row rowToCheck;
            bool exists = readRow(page, record.slotID, rowToCheck);
            Row insertedRow;
            std::memcpy(&insertedRow, record.after, ROW_SIZE);
            
            if (!exists || std::memcmp(&rowToCheck, &insertedRow, ROW_SIZE) != 0) {
                recoverInsertAt(page, record.slotID, insertedRow);
                disk.writePage(record.pageID, page);
            }
        } else if (record.op == WALOp::DELETE) {
            Row rowToCheck;
            bool exists = readRow(page, record.slotID, rowToCheck);
            if (exists) {
                deleteRow(page, record.slotID);
                disk.writePage(record.pageID, page);
            }
        } else if (record.op == WALOp::UPDATE) {
            Row updatedRow;
            std::memcpy(&updatedRow, record.after, ROW_SIZE);
            if (!updateRow(page, record.slotID, updatedRow)) {
                recoverInsertAt(page, record.slotID, updatedRow);
            }
            disk.writePage(record.pageID, page);
        }
    }
    
    checkpoint();
}

void WAL::checkpoint() {
    if (logFile.is_open()) {
        logFile.close();
    }
    logFile.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
    logFile.close();
    logFile.open(path, std::ios::in | std::ios::out | std::ios::app | std::ios::binary);
}
