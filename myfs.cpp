#include "myfs.h"
#include <string.h>
#include <iostream>
#include <math.h>
#include <sstream>

const char *MyFs::MYFS_MAGIC = "MYFS";

MyFs::MyFs(BlockDeviceSimulator *blkdevsim_)
	: _blockDeviceSim(blkdevsim_)
{
	readIndexTable();
	readINodeBitmap();

	struct myfs_header header;
	_blockDeviceSim->read(0, sizeof(header), (char *)&header);

	if (strncmp(header.magic, MYFS_MAGIC, sizeof(header.magic)) != 0 ||
		(header.version != CURR_VERSION))
	{
		std::cout << "Did not find myfs instance on blkdev" << std::endl;
		std::cout << "Creating..." << std::endl;
		format();
		std::cout << "Finished!" << std::endl;
	}
}

void MyFs::format()
{
	// Put the header in place
	struct myfs_header header;
	strncpy(header.magic, MYFS_MAGIC, sizeof(header.magic));
	header.version = CURR_VERSION;
	_blockDeviceSim->write(0, sizeof(header), (const char *)&header);

	std::vector<char> zeroBuffer(FILES_START - BITMAP_START, 0);
	_blockDeviceSim->write(BITMAP_START, zeroBuffer.size(), zeroBuffer.data());

	_lastFileAddress = FILES_START;

	writeIndexTable();
}

void MyFs::create_file(const std::string &path_str, bool isDirectory)
{
	if (checkINodeExists(path_str))
		return;

	int iNodeIndex = getEmptyINodeSlot();

	INodeEntry newFile = {
		iNodeIndex,
		"",
		isDirectory,
		0,
		0};

	strncpy(newFile.name, path_str.c_str(), sizeof(newFile.name) - 1);
	newFile.name[sizeof(newFile.name) - 1] = '\0'; // Ensure null termination

	int address = INODE_TABLE_START + (sizeof(INodeEntry) * newFile.index);
	_blockDeviceSim->write(address, sizeof(newFile), reinterpret_cast<char *>(&newFile));

	_iNodeBitmap[iNodeIndex] = 1;
	writeINodeBitmap();

	writeIndexTable();
}

std::string MyFs::get_content(const std::string &path_str)
{
	INodeEntry iNode = getINodeByName(path_str);

	std::vector<char> buffer(iNode.fileSize + 1, 0);
	_blockDeviceSim->read(iNode.contentAddress, iNode.fileSize, buffer.data());

	return std::string(buffer.data());
}

void MyFs::set_content(const std::string &path_str, const std::string &content)
{
	INodeEntry iNode = getINodeByName(path_str);

	// If the new content is longer, we need to find a new place for it
	if (content.size() > (size_t)iNode.fileSize)
	{
		iNode.contentAddress = _lastFileAddress;
	}

	// Update the file metadata
	iNode.fileSize = content.size();

	// Write the file content
	_blockDeviceSim->write(iNode.contentAddress, iNode.fileSize, content.c_str());

	// Write back the updated inode entry
	int inodeAddress = INODE_TABLE_START + (sizeof(INodeEntry) * iNode.index);
	_blockDeviceSim->write(inodeAddress, sizeof(INodeEntry), reinterpret_cast<char *>(&iNode));

	_lastFileAddress += iNode.fileSize;
}

MyFs::INodeList MyFs::list_dir(const std::string &path_str)
{
	INodeList answer;

	for (size_t i = 0; i < MAX_FILES; ++i)
	{
		if (!_iNodeBitmap[i])
			continue;

		INodeEntry iNode = getINodeAtIndex(i);

		if (iNode.name[0] == '\0')
			continue;

		answer.push_back(iNode);
	}

	return answer;
}

void MyFs::remove_file(const std::string &path_str)
{
	auto iNode = getINodeByName(path_str);

	int iNodeAddress = getINodeAddress(iNode.index);

	// Zero out the iNode
	char data[sizeof(iNode)] = {0};
	_blockDeviceSim->write(iNodeAddress, sizeof(iNode), data);

	// Zero out the file content
	std::vector<char> buffer(iNode.fileSize, 0);
	_blockDeviceSim->write(iNode.contentAddress, iNode.fileSize, buffer.data());

	_iNodeBitmap[iNode.index] = 0;
	writeINodeBitmap();
}

void MyFs::rename_file(const std::string &path_str, const std::string &new_str)
{
	auto iNode = getINodeByName(path_str);

	memset(iNode.name, 0, sizeof(iNode.name)); // Clear old name
	strncpy(iNode.name, new_str.c_str(), sizeof(iNode.name) - 1);
	iNode.name[sizeof(iNode.name) - 1] = '\0';

	_blockDeviceSim->write(getINodeAddress(iNode.index), sizeof(iNode), reinterpret_cast<char *>(&iNode));
}

void MyFs::writeIndexTable() const
{
	char data[sizeof(int) * 2] = {0};
	memcpy(data + sizeof(int), &_lastFileAddress, sizeof(int));

	_blockDeviceSim->write(INDEX_TABLE_START, sizeof(int) * 2, reinterpret_cast<char *>(data));
}

void MyFs::writeINodeBitmap() const
{
	// Uncomment to see the bitmap

	// for (size_t i = 0; i < MAX_FILES; i++)
	// {
	// 	if (i % 100 == 0)
	// 		std::cout << '\n';

	// 	std::cout << (_iNodeBitmap[i] == 1 ? '1' : '0');
	// }

	// std::cout << '\n';

	_blockDeviceSim->write(BITMAP_START, MAX_FILES, _iNodeBitmap);
}

void MyFs::readINodeBitmap()
{
	_blockDeviceSim->read(BITMAP_START, MAX_FILES, _iNodeBitmap);
}

void MyFs::readIndexTable()
{
	_blockDeviceSim->read(INDEX_TABLE_START + sizeof(int), sizeof(int), reinterpret_cast<char *>(&_lastFileAddress));
}

MyFs::INodeEntry MyFs::getINodeAtIndex(const int index) const
{
	int offset = getINodeAddress(index);

	char buffer[sizeof(INodeEntry)] = {0};
	_blockDeviceSim->read(offset, sizeof(INodeEntry), buffer);

	INodeEntry iNode;
	memcpy(&iNode, buffer, sizeof(INodeEntry));

	return iNode;
}

MyFs::INodeEntry MyFs::getINodeByName(const std::string &path_str) const
{
	for (size_t i = 0; i < MAX_FILES; i++)
	{
		if (!_iNodeBitmap[i])
			continue;

		INodeEntry iNode = getINodeAtIndex(i);

		if (iNode.name == path_str)
			return iNode;
	}

	throw std::invalid_argument("File does not exist!");
}

int MyFs::getINodeAddress(const int index) const
{
	return INODE_TABLE_START + (index * sizeof(INodeEntry));
}

bool MyFs::checkINodeExists(const std::string &path_str) const
{
	try
	{
		getINodeByName(path_str);
	}
	catch (const std::exception &e)
	{
		return false;
	}

	return true;
}

int MyFs::getEmptyINodeSlot() const
{
	for (size_t i = 0; i < MAX_FILES; i++)
	{
		if (!_iNodeBitmap[i])
			return i;
	}

	throw std::runtime_error("File system is full.");
}
