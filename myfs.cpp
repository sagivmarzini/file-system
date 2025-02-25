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

	struct myfs_header header;
	_blockDeviceSim->read(0, sizeof(header), (char *)&header);

	if (std::string(header.magic) != MYFS_MAGIC || header.version != CURR_VERSION)
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
	header.magic[sizeof(header.magic) - 1] = '\0'; // Ensure null termination
	header.version = CURR_VERSION;
	_blockDeviceSim->write(0, sizeof(header), (const char *)&header);

	std::vector<char> zeroBuffer(FILES_START - BITMAP_START, 0);
	_blockDeviceSim->write(BITMAP_START, zeroBuffer.size(), zeroBuffer.data());

	_lastFileAddress = FILES_START;

	updateIndexTable();
}

void MyFs::create_file(const std::string &path_str, bool isDirectory)
{
	if (checkINodeExists(path_str))
		throw std::invalid_argument("File already exists");

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

	updateIndexTable();
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
		// Assign a new address for the new content
		iNode.contentAddress = _lastFileAddress;
		_lastFileAddress += content.size();

		// Delete the old content
		std::vector<char> zeros(iNode.fileSize, 0);
		_blockDeviceSim->write(iNode.contentAddress, iNode.fileSize, zeros.data());
	}

	// Update the file metadata
	iNode.fileSize = content.size();

	// Write the file content
	_blockDeviceSim->write(iNode.contentAddress, iNode.fileSize, content.c_str());

	// Write back the updated inode entry
	int inodeAddress = INODE_TABLE_START + (sizeof(INodeEntry) * iNode.index);
	_blockDeviceSim->write(inodeAddress, sizeof(INodeEntry), reinterpret_cast<char *>(&iNode));
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
	if (path_str == new_str)
		throw std::invalid_argument("New name is the same as the current name");
	if (checkINodeExists(path_str))
		throw std::invalid_argument("File already exists");

	auto iNode = getINodeByName(path_str);

	memset(iNode.name, 0, sizeof(iNode.name)); // Clear old name
	strncpy(iNode.name, new_str.c_str(), sizeof(iNode.name) - 1);
	iNode.name[sizeof(iNode.name) - 1] = '\0';

	_blockDeviceSim->write(getINodeAddress(iNode.index), sizeof(iNode), reinterpret_cast<char *>(&iNode));
}

void MyFs::updateIndexTable() const
{
	_blockDeviceSim->write(INDEX_TABLE_START, sizeof(_lastFileAddress), reinterpret_cast<char *>(_lastFileAddress));
}

void MyFs::writeINodeBitmap() const
{
	_blockDeviceSim->write(BITMAP_START, MAX_FILES, _iNodeBitmap);
}

void MyFs::readIndexTable()
{
	_blockDeviceSim->read(INDEX_TABLE_START + sizeof(_lastFileAddress), sizeof(_lastFileAddress), reinterpret_cast<char *>(&_lastFileAddress));
}

MyFs::INodeEntry MyFs::getINodeAtIndex(const int index) const
{
	int offset = getINodeAddress(index);

	INodeEntry iNode;
	_blockDeviceSim->read(offset, sizeof(INodeEntry), reinterpret_cast<char *>(&iNode));

	return iNode;
}

MyFs::INodeEntry MyFs::getINodeByName(const std::string &path_str) const
{
	for (size_t i = 0; i < MAX_FILES; i++)
	{
		if (!_iNodeBitmap[i])
			continue;

		INodeEntry iNode = getINodeAtIndex(i);

		if (strncmp(iNode.name, path_str.c_str(), sizeof(iNode.name)) == 0)
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
	catch (...)
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
