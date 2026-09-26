#ifndef PAGE_H
#define PAGE_H
#include <cstdint>
#include <cstring>
#include "Row.h"

using PageID = uint32_t;
constexpr uint32_t PAGE_HEADER_SIZE = 8;

struct Page
{
    uint8_t data[PAGE_SIZE];
    PageID id;
};

inline bool insertRow(Page& page, const Row& row, uint16_t& slotID)
{
    uint32_t slotCount;
    uint32_t freeSpaceOffset;
    std::memcpy(&slotCount, page.data + PAGE_SIZE - 8, sizeof(slotCount));
    std::memcpy(&freeSpaceOffset, page.data + PAGE_SIZE - 4, sizeof(freeSpaceOffset));

    const uint32_t slotDirectoryEnd = PAGE_SIZE - PAGE_HEADER_SIZE -
        slotCount * sizeof(uint16_t) * 2;
    if (freeSpaceOffset + ROW_SIZE + sizeof(uint16_t) * 2 > slotDirectoryEnd)
    {
        return false;
    }

    std::memcpy(page.data + freeSpaceOffset, &row, ROW_SIZE);

    const uint16_t rowOffset = static_cast<uint16_t>(freeSpaceOffset);
    const uint16_t rowLength = static_cast<uint16_t>(ROW_SIZE);
    const uint32_t slotEntryOffset = PAGE_SIZE - PAGE_HEADER_SIZE -
        (slotCount + 1) * sizeof(uint16_t) * 2;
    std::memcpy(page.data + slotEntryOffset, &rowOffset, sizeof(rowOffset));
    std::memcpy(page.data + slotEntryOffset + sizeof(rowOffset),
                &rowLength, sizeof(rowLength));

    slotID = static_cast<uint16_t>(slotCount);
    ++slotCount;
    freeSpaceOffset += ROW_SIZE;
    std::memcpy(page.data + PAGE_SIZE - 8, &slotCount, sizeof(slotCount));
    std::memcpy(page.data + PAGE_SIZE - 4, &freeSpaceOffset, sizeof(freeSpaceOffset));
    return true;
}

inline bool readRow(const Page& page, uint16_t slotID, Row& row)
{
    uint32_t slotCount;
    std::memcpy(&slotCount, page.data + PAGE_SIZE - PAGE_HEADER_SIZE,
                sizeof(slotCount));
    if (slotID >= slotCount)
    {
        return false;
    }

    const uint32_t slotEntryOffset = PAGE_SIZE - PAGE_HEADER_SIZE -
        (static_cast<uint32_t>(slotID) + 1) * sizeof(uint16_t) * 2;
    uint16_t rowOffset;
    uint16_t rowLength;
    std::memcpy(&rowOffset, page.data + slotEntryOffset, sizeof(rowOffset));
    std::memcpy(&rowLength, page.data + slotEntryOffset + sizeof(rowOffset),
                sizeof(rowLength));

    if (rowOffset == UINT16_MAX || rowLength != ROW_SIZE ||
        static_cast<uint32_t>(rowOffset) + rowLength > PAGE_SIZE - PAGE_HEADER_SIZE)
    {
        return false;
    }

    std::memcpy(&row, page.data + rowOffset, ROW_SIZE);
    return true;
}

inline void deleteRow(Page& page, uint16_t slotID)
{
    uint32_t slotCount;
    std::memcpy(&slotCount, page.data + PAGE_SIZE - PAGE_HEADER_SIZE,
                sizeof(slotCount));
    if (slotID >= slotCount)
    {
        return;
    }

    const uint32_t slotEntryOffset = PAGE_SIZE - PAGE_HEADER_SIZE -
        (static_cast<uint32_t>(slotID) + 1) * sizeof(uint16_t) * 2;
    const uint16_t tombstone = UINT16_MAX;
    std::memcpy(page.data + slotEntryOffset, &tombstone, sizeof(tombstone));
}

inline bool updateRow(Page& page, uint16_t slotID, const Row& row)
{
    uint32_t slotCount;
    std::memcpy(&slotCount, page.data + PAGE_SIZE - PAGE_HEADER_SIZE,
                sizeof(slotCount));
    if (slotID >= slotCount)
    {
        return false;
    }

    const uint32_t slotEntryOffset = PAGE_SIZE - PAGE_HEADER_SIZE -
        (static_cast<uint32_t>(slotID) + 1) * sizeof(uint16_t) * 2;
    uint16_t rowOffset;
    uint16_t rowLength;
    std::memcpy(&rowOffset, page.data + slotEntryOffset, sizeof(rowOffset));
    std::memcpy(&rowLength, page.data + slotEntryOffset + sizeof(rowOffset),
                sizeof(rowLength));

    if (rowOffset == UINT16_MAX || rowLength != ROW_SIZE ||
        static_cast<uint32_t>(rowOffset) + rowLength > PAGE_SIZE - PAGE_HEADER_SIZE)
    {
        return false;
    }

    std::memcpy(page.data + rowOffset, &row, ROW_SIZE);
    return true;
}

inline bool recoverInsertAt(Page& page, uint16_t slotID, const Row& row)
{
    uint32_t slotCount = 0;
    uint32_t freeSpaceOffset = 0;
    std::memcpy(&slotCount, page.data + PAGE_SIZE - PAGE_HEADER_SIZE,
                sizeof(slotCount));
    std::memcpy(&freeSpaceOffset, page.data + PAGE_SIZE - 4,
                sizeof(freeSpaceOffset));
    if (slotID > slotCount || slotCount > MAX_SLOTS || freeSpaceOffset > PAGE_SIZE - PAGE_HEADER_SIZE)
    {
        return false;
    }

    if (slotID == slotCount)
    {
        uint16_t insertedSlot = 0;
        return insertRow(page, row, insertedSlot) && insertedSlot == slotID;
    }

    const uint32_t slotEntryOffset = PAGE_SIZE - PAGE_HEADER_SIZE -
        (static_cast<uint32_t>(slotID) + 1) * sizeof(uint16_t) * 2;
    uint16_t rowOffset = 0;
    uint16_t rowLength = 0;
    std::memcpy(&rowOffset, page.data + slotEntryOffset, sizeof(rowOffset));
    std::memcpy(&rowLength, page.data + slotEntryOffset + sizeof(rowOffset), sizeof(rowLength));
    if (rowOffset != UINT16_MAX && rowLength == ROW_SIZE &&
        static_cast<uint32_t>(rowOffset) + rowLength <= PAGE_SIZE - PAGE_HEADER_SIZE)
    {
        std::memcpy(page.data + rowOffset, &row, ROW_SIZE);
        return true;
    }

    const uint32_t slotDirectoryStart = PAGE_SIZE - PAGE_HEADER_SIZE -
        slotCount * sizeof(uint16_t) * 2;
    if (freeSpaceOffset + ROW_SIZE > slotDirectoryStart || freeSpaceOffset > UINT16_MAX)
    {
        return false;
    }
    std::memcpy(page.data + freeSpaceOffset, &row, ROW_SIZE);
    rowOffset = static_cast<uint16_t>(freeSpaceOffset);
    rowLength = static_cast<uint16_t>(ROW_SIZE);
    std::memcpy(page.data + slotEntryOffset, &rowOffset, sizeof(rowOffset));
    std::memcpy(page.data + slotEntryOffset + sizeof(rowOffset), &rowLength, sizeof(rowLength));
    freeSpaceOffset += ROW_SIZE;
    std::memcpy(page.data + PAGE_SIZE - 4, &freeSpaceOffset, sizeof(freeSpaceOffset));
    return true;
}

inline uint16_t getSlotCount(const Page& page)
{
    uint32_t slotCount = 0;
    std::memcpy(&slotCount, page.data + PAGE_SIZE - PAGE_HEADER_SIZE,
                sizeof(slotCount));
    return static_cast<uint16_t>(slotCount);
}

inline bool hasSpace(const Page& page)
{
    uint32_t slotCount = 0;
    uint32_t freeSpaceOffset = 0;
    std::memcpy(&slotCount, page.data + PAGE_SIZE - PAGE_HEADER_SIZE,
                sizeof(slotCount));
    std::memcpy(&freeSpaceOffset, page.data + PAGE_SIZE - 4,
                sizeof(freeSpaceOffset));

    const uint32_t slotDirectoryEnd = PAGE_SIZE - PAGE_HEADER_SIZE -
        slotCount * sizeof(uint16_t) * 2;
    return freeSpaceOffset + ROW_SIZE + sizeof(uint16_t) * 2 <= slotDirectoryEnd;
}
#endif // PAGE_H
