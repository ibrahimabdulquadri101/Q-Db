#include "BTree.h"

#include "DiskManager.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace
{
constexpr std::size_t NODE_DATA_SIZE = sizeof(uint8_t) + sizeof(int32_t) +
	BTREE_ORDER * sizeof(int32_t) + (BTREE_ORDER + 1) * sizeof(PageID) +
	BTREE_ORDER * (sizeof(PageID) + sizeof(uint16_t)) + sizeof(PageID);
static_assert(NODE_DATA_SIZE <= PAGE_SIZE, "B-tree node must fit in one page");

template <typename Value>
void writeValue(uint8_t *data, std::size_t &offset, const Value &value)
{
	std::memcpy(data + offset, &value, sizeof(Value));
	offset += sizeof(Value);
}

template <typename Value>
Value readValue(const uint8_t *data, std::size_t &offset)
{
	Value value;
	std::memcpy(&value, data + offset, sizeof(Value));
	offset += sizeof(Value);
	return value;
}

std::string rootManifestPath(const DiskManager *disk, const std::string &key)
{
	std::string safeKey = key;
	for (char &character : safeKey)
	{
		if (!((character >= 'a' && character <= 'z') ||
			  (character >= 'A' && character <= 'Z') ||
			  (character >= '0' && character <= '9') || character == '_' || character == '-'))
		{
			character = '_';
		}
	}
	return disk->getFilePath() + "." + safeKey + ".btree";
}

PageID readRootManifest(const std::string &path)
{
	std::ifstream manifest(path, std::ios::binary);
	PageID root = 0;
	if (manifest)
	{
		manifest.read(reinterpret_cast<char *>(&root), sizeof(root));
		if (!manifest)
		{
			throw std::runtime_error("Corrupt B-tree root manifest");
		}
	}
	return root;
}
}

BTree::BTree() : disk(nullptr), rootPageID(0), storageKey("default") {}

void BTree::init(DiskManager *diskManager)
{
	init(diskManager, "default");
}

void BTree::init(DiskManager *diskManager, std::string key)
{
	if (diskManager == nullptr)
	{
		throw std::invalid_argument("B-tree requires a disk manager");
	}
	disk = diskManager;
	storageKey = std::move(key);
	rootPageID = readRootManifest(rootManifestPath(disk, storageKey));
	if (rootPageID == 0)
	{
		BTreeNode root{};
		root.isLeaf = true;
		rootPageID = disk->allocatePage();
		writeNode(rootPageID, root);
		saveRoot();
	}
	else
	{
		readNode(rootPageID);
	}
}

BTreeNode BTree::readNode(PageID pageID) const
{
	if (disk == nullptr || pageID == 0)
	{
		throw std::runtime_error("Invalid B-tree page reference");
	}
	Page page{};
	if (!disk->readPage(pageID, page))
	{
		throw std::runtime_error("Failed to read B-tree node page");
	}
	BTreeNode node{};
	std::size_t offset = 0;
	node.isLeaf = readValue<uint8_t>(page.data, offset) != 0;
	node.keyCount = readValue<int32_t>(page.data, offset);
	if (node.keyCount < 0 || node.keyCount > BTREE_ORDER)
	{
		throw std::runtime_error("Corrupt B-tree node key count");
	}
	for (int i = 0; i < BTREE_ORDER; ++i)
	{
		node.keys[i] = readValue<int32_t>(page.data, offset);
	}
	for (int i = 0; i < BTREE_ORDER + 1; ++i)
	{
		node.children[i] = readValue<PageID>(page.data, offset);
	}
	for (int i = 0; i < BTREE_ORDER; ++i)
	{
		node.rowPointers[i].first = readValue<PageID>(page.data, offset);
		node.rowPointers[i].second = readValue<uint16_t>(page.data, offset);
	}
	node.nextLeaf = readValue<PageID>(page.data, offset);
	return node;
}

void BTree::writeNode(PageID pageID, const BTreeNode &node) const
{
	if (disk == nullptr || pageID == 0 || node.keyCount < 0 || node.keyCount > BTREE_ORDER)
	{
		throw std::runtime_error("Invalid B-tree node write");
	}
	Page page{};
	page.id = pageID;
	std::size_t offset = 0;
	const uint8_t isLeaf = node.isLeaf ? 1 : 0;
	const int32_t keyCount = node.keyCount;
	writeValue(page.data, offset, isLeaf);
	writeValue(page.data, offset, keyCount);
	for (int i = 0; i < BTREE_ORDER; ++i)
	{
		writeValue(page.data, offset, node.keys[i]);
	}
	for (int i = 0; i < BTREE_ORDER + 1; ++i)
	{
		writeValue(page.data, offset, node.children[i]);
	}
	for (int i = 0; i < BTREE_ORDER; ++i)
	{
		writeValue(page.data, offset, node.rowPointers[i].first);
		writeValue(page.data, offset, node.rowPointers[i].second);
	}
	writeValue(page.data, offset, node.nextLeaf);
	if (!disk->writePage(pageID, page))
	{
		throw std::runtime_error("Failed to write B-tree node page");
	}
}

void BTree::saveRoot() const
{
	const std::string path = rootManifestPath(disk, storageKey);
	const std::string temporaryPath = path + ".tmp";
	std::ofstream manifest(temporaryPath, std::ios::binary | std::ios::trunc);
	manifest.write(reinterpret_cast<const char *>(&rootPageID), sizeof(rootPageID));
	manifest.close();
	if (!manifest)
	{
		throw std::runtime_error("Failed to save B-tree root manifest");
	}
	std::remove(path.c_str());
	if (std::rename(temporaryPath.c_str(), path.c_str()) != 0)
	{
		throw std::runtime_error("Failed to replace B-tree root manifest");
	}
}

PageID BTree::findLeaf(int32_t key) const
{
	PageID pageID = rootPageID;
	BTreeNode node = readNode(pageID);
	while (!node.isLeaf)
	{
		int childIndex = 0;
		while (childIndex < node.keyCount && key > node.keys[childIndex])
		{
			++childIndex;
		}
		pageID = node.children[childIndex];
		node = readNode(pageID);
	}
	return pageID;
}

void BTree::insert(int32_t key, PageID rowPageID, uint16_t slotID)
{
	if (disk == nullptr || rootPageID == 0)
	{
		throw std::runtime_error("B-tree is not initialized");
	}
	std::vector<std::pair<PageID, BTreeNode>> ancestors;
	PageID leafPageID = rootPageID;
	BTreeNode leaf = readNode(leafPageID);
	while (!leaf.isLeaf)
	{
		ancestors.push_back({leafPageID, leaf});
		int childIndex = 0;
		while (childIndex < leaf.keyCount && key > leaf.keys[childIndex])
		{
			++childIndex;
		}
		leafPageID = leaf.children[childIndex];
		leaf = readNode(leafPageID);
	}

	int insertionPosition = 0;
	while (insertionPosition < leaf.keyCount && leaf.keys[insertionPosition] <= key)
	{
		++insertionPosition;
	}
	if (leaf.keyCount < BTREE_ORDER)
	{
		for (int i = leaf.keyCount; i > insertionPosition; --i)
		{
			leaf.keys[i] = leaf.keys[i - 1];
			leaf.rowPointers[i] = leaf.rowPointers[i - 1];
		}
		leaf.keys[insertionPosition] = key;
		leaf.rowPointers[insertionPosition] = {rowPageID, slotID};
		++leaf.keyCount;
		writeNode(leafPageID, leaf);
		return;
	}

	std::vector<int32_t> leafKeys(BTREE_ORDER + 1);
	std::vector<std::pair<PageID, uint16_t>> leafPointers(BTREE_ORDER + 1);
	for (int source = 0, target = 0; target < BTREE_ORDER + 1; ++target)
	{
		if (target == insertionPosition)
		{
			leafKeys[target] = key;
			leafPointers[target] = {rowPageID, slotID};
		}
		else
		{
			leafKeys[target] = leaf.keys[source];
			leafPointers[target] = leaf.rowPointers[source++];
		}
	}

	BTreeNode rightLeaf{};
	rightLeaf.isLeaf = true;
	const int leftCount = (BTREE_ORDER + 1) / 2;
	leaf.keyCount = leftCount;
	for (int i = 0; i < leftCount; ++i)
	{
		leaf.keys[i] = leafKeys[i];
		leaf.rowPointers[i] = leafPointers[i];
	}
	rightLeaf.keyCount = BTREE_ORDER + 1 - leftCount;
	for (int i = 0; i < rightLeaf.keyCount; ++i)
	{
		rightLeaf.keys[i] = leafKeys[leftCount + i];
		rightLeaf.rowPointers[i] = leafPointers[leftCount + i];
	}
	const PageID oldNextLeaf = leaf.nextLeaf;
	const PageID rightLeafPageID = disk->allocatePage();
	rightLeaf.nextLeaf = oldNextLeaf;
	leaf.nextLeaf = rightLeafPageID;
	writeNode(leafPageID, leaf);
	writeNode(rightLeafPageID, rightLeaf);
	int32_t separator = rightLeaf.keys[0];
	PageID leftPageID = leafPageID;
	PageID rightPageID = rightLeafPageID;

	while (!ancestors.empty())
	{
		auto parentEntry = ancestors.back();
		ancestors.pop_back();
		PageID parentPageID = parentEntry.first;
		BTreeNode parent = readNode(parentPageID);
		int childPosition = 0;
		while (childPosition <= parent.keyCount && parent.children[childPosition] != leftPageID)
		{
			++childPosition;
		}
		if (childPosition > parent.keyCount)
		{
			throw std::runtime_error("B-tree parent does not reference split child");
		}

		if (parent.keyCount < BTREE_ORDER)
		{
			for (int i = parent.keyCount; i > childPosition; --i)
			{
				parent.keys[i] = parent.keys[i - 1];
			}
			for (int i = parent.keyCount + 1; i > childPosition + 1; --i)
			{
				parent.children[i] = parent.children[i - 1];
			}
			parent.keys[childPosition] = separator;
			parent.children[childPosition + 1] = rightPageID;
			++parent.keyCount;
			writeNode(parentPageID, parent);
			return;
		}

		std::vector<int32_t> parentKeys(BTREE_ORDER + 1);
		std::vector<PageID> parentChildren(BTREE_ORDER + 2);
		for (int i = 0, source = 0; i < BTREE_ORDER + 1; ++i)
		{
			if (i == childPosition) parentKeys[i] = separator;
			else parentKeys[i] = parent.keys[source++];
		}
		for (int i = 0, source = 0; i < BTREE_ORDER + 2; ++i)
		{
			if (i == childPosition + 1) parentChildren[i] = rightPageID;
			else parentChildren[i] = parent.children[source++];
		}

		const int middle = (BTREE_ORDER + 1) / 2;
		const int32_t promoted = parentKeys[middle];
		parent.keyCount = middle;
		for (int i = 0; i < middle; ++i) parent.keys[i] = parentKeys[i];
		for (int i = 0; i <= middle; ++i) parent.children[i] = parentChildren[i];

		BTreeNode rightInternal{};
		rightInternal.isLeaf = false;
		rightInternal.keyCount = BTREE_ORDER - middle;
		for (int i = 0; i < rightInternal.keyCount; ++i)
		{
			rightInternal.keys[i] = parentKeys[middle + 1 + i];
		}
		for (int i = 0; i <= rightInternal.keyCount; ++i)
		{
			rightInternal.children[i] = parentChildren[middle + 1 + i];
		}
		const PageID rightInternalPageID = disk->allocatePage();
		writeNode(parentPageID, parent);
		writeNode(rightInternalPageID, rightInternal);
		separator = promoted;
		leftPageID = parentPageID;
		rightPageID = rightInternalPageID;
	}

	BTreeNode newRoot{};
	newRoot.isLeaf = false;
	newRoot.keyCount = 1;
	newRoot.keys[0] = separator;
	newRoot.children[0] = leftPageID;
	newRoot.children[1] = rightPageID;
	rootPageID = disk->allocatePage();
	writeNode(rootPageID, newRoot);
	saveRoot();
}

bool BTree::search(int32_t key, PageID &pageID, uint16_t &slotID)
{
	PageID leafPageID = findLeaf(key);
	BTreeNode leaf = readNode(leafPageID);
	while (leafPageID != 0)
	{
		for (int i = 0; i < leaf.keyCount; ++i)
		{
			if (leaf.keys[i] == key)
			{
				pageID = leaf.rowPointers[i].first;
				 slotID = leaf.rowPointers[i].second;
				return true;
			}
			if (leaf.keys[i] > key)
			{
				return false;
			}
		}
		if (leaf.nextLeaf == 0)
		{
			break;
		}
		leafPageID = leaf.nextLeaf;
		leaf = readNode(leafPageID);
		if (leaf.keyCount > 0 && leaf.keys[0] > key)
		{
			return false;
		}
	}
	return false;
}

void BTree::rangeSearch(int32_t minKey, std::function<void(PageID, uint16_t)> callback, int32_t maxKey)
{
	if (!callback)
	{
		return;
	}
	PageID leafPageID = findLeaf(minKey);
	BTreeNode leaf = readNode(leafPageID);
	while (leafPageID != 0)
	{
		for (int i = 0; i < leaf.keyCount; ++i)
		{
			if (leaf.keys[i] > maxKey)
			{
				return;
			}
			if (leaf.keys[i] >= minKey)
			{
				callback(leaf.rowPointers[i].first, leaf.rowPointers[i].second);
			}
		}
		if (leaf.nextLeaf == 0)
		{
			break;
		}
		leafPageID = leaf.nextLeaf;
		leaf = readNode(leafPageID);
	}
}

bool BTree::empty() const
{
	return rootPageID == 0 || readNode(rootPageID).keyCount == 0;
}
