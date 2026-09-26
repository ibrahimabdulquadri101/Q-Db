#ifndef HASHINDEX_H
#define HASHINDEX_H

#include "Page.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

class DiskManager;

class HashIndex
{
private:
	std::unordered_map<std::string, std::pair<PageID, uint16_t>> map;
	std::string indexedColumn;

public:
	void init(std::string column);
	void insert(std::string value, PageID pageID, uint16_t slotID);
	bool lookup(std::string value, PageID &pageID, uint16_t &slotID);
	void remove(std::string value);
	void saveToPages(DiskManager &disk);
	void loadFromPages(DiskManager &disk);
	bool empty() const;
};

#endif // HASHINDEX_H
