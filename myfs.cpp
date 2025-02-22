#include "myfs.h"
#include <string.h>
#include <iostream>
#include <math.h>
#include <sstream>

constexpr auto MAX_FILES = 1028;

constexpr auto INODE_TABLE_START = sizeof(MyFs::myfs_header);

constexpr auto INDEX_TABLE_START = INODE_TABLE_START + (sizeof(MyFs::INodeEntry) * MAX_FILES);

constexpr auto FILES_START = INDEX_TABLE_START + sizeof(int) * 2;

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

	// Zero out inode table and index table to avoid garbage data
	char zero_buffer[INDEX_TABLE_START - sizeof(header)] = {0};
	_blockDeviceSim->write(sizeof(header), sizeof(zero_buffer), zero_buffer);

	// Zero out index table
	char zero_index[sizeof(int) * 2] = {0};
	_blockDeviceSim->write(INDEX_TABLE_START, sizeof(zero_index), zero_index);

	_lastINodeIndex = 0;
	_lastFileAddress = FILES_START;

	updateIndexTable();
}

void MyFs::create_file(const std::string &path_str, bool isDirectory)
{
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

	char buffer[iNode.fileSize] = {0};
	_blockDeviceSim->read(iNode.contentAddress, iNode.fileSize, buffer);
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
	int offset = INODE_TABLE_START + (index * sizeof(INodeEntry));

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
