#ifndef BTREE_H
#define BTREE_H

#include "Page.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

class DiskManager;

const int BTREE_ORDER = 50;

struct BTreeNode
{
	bool isLeaf;
	int keyCount;
	int32_t keys[BTREE_ORDER];
	PageID children[BTREE_ORDER + 1];
	std::pair<PageID, uint16_t> rowPointers[BTREE_ORDER];
	PageID nextLeaf;
};

class BTree
{
private:
	DiskManager *disk;
	PageID rootPageID;
	std::string storageKey;

	BTreeNode readNode(PageID pageID) const;
	void writeNode(PageID pageID, const BTreeNode &node) const;
	PageID findLeaf(int32_t key) const;
	void saveRoot() const;

public:
	BTree();
	void init(DiskManager *disk);
	void init(DiskManager *disk, std::string storageKey);
	void insert(int32_t key, PageID pageID, uint16_t slotID);
	bool search(int32_t key, PageID &pageID, uint16_t &slotID);
	void rangeSearch(int32_t minKey, std::function<void(PageID, uint16_t)> callback, int32_t maxKey = 2147483647);
	bool empty() const;
};

#endif // BTREE_H
