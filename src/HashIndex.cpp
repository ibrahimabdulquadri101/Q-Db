#include "HashIndex.h"

#include "DiskManager.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace
{
std::string manifestPath(const DiskManager &disk, const std::string &column)
{
	std::string safeColumn = column;
	for (char &character : safeColumn)
	{
		if (!std::isalnum(static_cast<unsigned char>(character)) &&
			character != '_' && character != '-')
		{
			character = '_';
		}
	}
	return disk.getFilePath() + "." + safeColumn + ".hidx";
}

template <typename Value>
void appendValue(std::vector<char> &bytes, const Value &value)
{
	const char *data = reinterpret_cast<const char *>(&value);
	bytes.insert(bytes.end(), data, data + sizeof(Value));
}

template <typename Value>
Value readValue(const std::vector<char> &bytes, std::size_t &position)
{
	if (position + sizeof(Value) > bytes.size())
	{
		throw std::runtime_error("Corrupt hash index data");
	}
	Value value;
	std::memcpy(&value, bytes.data() + position, sizeof(Value));
	position += sizeof(Value);
	return value;
}

std::vector<PageID> readManifest(const std::string &path)
{
	std::ifstream manifest(path, std::ios::binary);
	if (!manifest)
	{
		return {};
	}

	uint32_t pageCount = 0;
	manifest.read(reinterpret_cast<char *>(&pageCount), sizeof(pageCount));
	if (!manifest || pageCount > 1000000)
	{
		throw std::runtime_error("Corrupt hash index manifest");
	}
	std::vector<PageID> pageIDs(pageCount);
	manifest.read(reinterpret_cast<char *>(pageIDs.data()),
				  static_cast<std::streamsize>(pageIDs.size() * sizeof(PageID)));
	if (!manifest && pageCount != 0)
	{
		throw std::runtime_error("Corrupt hash index manifest");
	}
	return pageIDs;
}
}

void HashIndex::init(std::string column)
{
	indexedColumn = std::move(column);
	map.clear();
}

void HashIndex::insert(std::string value, PageID pageID, uint16_t slotID)
{
	map[std::move(value)] = {pageID, slotID};
}

bool HashIndex::lookup(std::string value, PageID &pageID, uint16_t &slotID)
{
	const auto found = map.find(value);
	if (found == map.end())
	{
		return false;
	}
	pageID = found->second.first;
	slotID = found->second.second;
	return true;
}

void HashIndex::remove(std::string value)
{
	map.erase(value);
}

void HashIndex::saveToPages(DiskManager &disk)
{
	const std::string path = manifestPath(disk, indexedColumn);
	const std::vector<PageID> oldPages = readManifest(path);
	for (PageID pageID : oldPages)
	{
		disk.freePage(pageID);
	}

	std::vector<char> bytes;
	const uint32_t entryCount = static_cast<uint32_t>(map.size());
	appendValue(bytes, entryCount);
	for (const auto &[value, location] : map)
	{
		const uint32_t valueLength = static_cast<uint32_t>(value.size());
		appendValue(bytes, valueLength);
		bytes.insert(bytes.end(), value.begin(), value.end());
		appendValue(bytes, location.first);
		appendValue(bytes, location.second);
	}

	const std::size_t pageCount = (bytes.size() + PAGE_SIZE - 1) / PAGE_SIZE;
	std::vector<PageID> pageIDs;
	pageIDs.reserve(pageCount);
	for (std::size_t pageIndex = 0; pageIndex < pageCount; ++pageIndex)
	{
		const PageID pageID = disk.allocatePage();
		Page page{};
		page.id = pageID;
		const std::size_t start = pageIndex * PAGE_SIZE;
		const std::size_t length = std::min<std::size_t>(PAGE_SIZE, bytes.size() - start);
		std::memcpy(page.data, bytes.data() + start, length);
		if (!disk.writePage(pageID, page))
		{
			throw std::runtime_error("Failed to write hash index page");
		}
		pageIDs.push_back(pageID);
	}

	const std::string temporaryPath = path + ".tmp";
	std::ofstream manifest(temporaryPath, std::ios::binary | std::ios::trunc);
	const uint32_t savedPageCount = static_cast<uint32_t>(pageIDs.size());
	manifest.write(reinterpret_cast<const char *>(&savedPageCount), sizeof(savedPageCount));
	manifest.write(reinterpret_cast<const char *>(pageIDs.data()),
				   static_cast<std::streamsize>(pageIDs.size() * sizeof(PageID)));
	manifest.close();
	if (!manifest)
	{
		throw std::runtime_error("Failed to write hash index manifest");
	}
	std::remove(path.c_str());
	if (std::rename(temporaryPath.c_str(), path.c_str()) != 0)
	{
		throw std::runtime_error("Failed to replace hash index manifest");
	}
}

void HashIndex::loadFromPages(DiskManager &disk)
{
	map.clear();
	const std::vector<PageID> pageIDs = readManifest(manifestPath(disk, indexedColumn));
	if (pageIDs.empty())
	{
		return;
	}

	std::vector<char> bytes;
	bytes.reserve(pageIDs.size() * PAGE_SIZE);
	for (PageID pageID : pageIDs)
	{
		Page page{};
		if (!disk.readPage(pageID, page))
		{
			throw std::runtime_error("Failed to read hash index page");
		}
		bytes.insert(bytes.end(), reinterpret_cast<const char *>(page.data),
					 reinterpret_cast<const char *>(page.data) + PAGE_SIZE);
	}

	std::size_t position = 0;
	const uint32_t entryCount = readValue<uint32_t>(bytes, position);
	if (entryCount > bytes.size())
	{
		throw std::runtime_error("Corrupt hash index entry count");
	}
	for (uint32_t entry = 0; entry < entryCount; ++entry)
	{
		const uint32_t valueLength = readValue<uint32_t>(bytes, position);
		if (valueLength > bytes.size() - position)
		{
			throw std::runtime_error("Corrupt hash index value");
		}
		std::string value(bytes.data() + position, valueLength);
		position += valueLength;
		const PageID pageID = readValue<PageID>(bytes, position);
		const uint16_t slotID = readValue<uint16_t>(bytes, position);
		map[std::move(value)] = {pageID, slotID};
	}
}

bool HashIndex::empty() const
{
	return map.empty();
}
