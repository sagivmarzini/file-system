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

	// Format the iNode bitmap
	_iNodeBitmap[0] = 1; // The header block is taken
	_iNodeBitmap[1] = 1;

	// Zero out inode table and index table to avoid garbage data
	char zeroBuffer[INDEX_TABLE_START - sizeof(header)] = {0};
	_blockDeviceSim->write(sizeof(header), sizeof(zeroBuffer), zeroBuffer);

	// Zero out index table
	char zeroIndex[sizeof(int) * 2] = {0};
	_blockDeviceSim->write(INDEX_TABLE_START, sizeof(zeroIndex), zeroIndex);

	_lastINodeIndex = 0;
	_lastFileAddress = FILES_START;

	updateIndexTable();
}

void MyFs::create_file(const std::string &path_str, bool isDirectory)
{
	if (checkINodeExists(path_str))
		return;

	INodeEntry newFile = {
		_lastINodeIndex++,
		"",
		isDirectory,
		0,
		0};

	strncpy(newFile.name, path_str.c_str(), sizeof(newFile.name) - 1);
	newFile.name[sizeof(newFile.name) - 1] = '\0'; // Ensure null termination

	int address = INODE_TABLE_START + (sizeof(INodeEntry) * newFile.index);
	_blockDeviceSim->write(address, sizeof(newFile), reinterpret_cast<char *>(&newFile));

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

	// Update the file metadata
	iNode.fileSize = content.size();
	iNode.contentAddress = _lastFileAddress;

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

	for (size_t i = 0; i < (size_t)_lastINodeIndex; ++i)
	{
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
}

void MyFs::rename_file(const std::string &path_str, const std::string &new_str)
{
	auto iNode = getINodeByName(path_str);

	memset(iNode.name, 0, sizeof(iNode.name)); // Clear old name
	strncpy(iNode.name, new_str.c_str(), sizeof(iNode.name) - 1);
	iNode.name[sizeof(iNode.name) - 1] = '\0';

	_blockDeviceSim->write(getINodeAddress(iNode.index), sizeof(iNode), reinterpret_cast<char *>(&iNode));
}

void MyFs::updateIndexTable() const
{
	char data[sizeof(int) * 2] = {0};
	memcpy(data, &_lastINodeIndex, sizeof(int));
	memcpy(data + sizeof(int), &_lastFileAddress, sizeof(int));

	_blockDeviceSim->write(INDEX_TABLE_START, sizeof(int) * 2, reinterpret_cast<char *>(data));
}

void MyFs::readIndexTable()
{
	_blockDeviceSim->read(INDEX_TABLE_START, sizeof(int), reinterpret_cast<char *>(&_lastINodeIndex));
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
	INodeEntry iNode;

	for (size_t i = 0; i < (size_t)_lastINodeIndex; i++)
	{
		iNode = getINodeAtIndex(i);

		if (iNode.name == path_str)
			break;
	}

	if (iNode.name != path_str)
		throw std::invalid_argument("File does not exist!");

	return iNode;
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
