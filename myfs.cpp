#include "myfs.h"
#include <string.h>
#include <iostream>
#include <math.h>
#include <sstream>

const char *MyFs::MYFS_MAGIC = "MYFS";

MyFs::MyFs(BlockDeviceSimulator *blkdevsim_) : blockDeviceSim(blkdevsim_)
{
	struct myfs_header header;
	blockDeviceSim->read(0, sizeof(header), (char *)&header);

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
	blockDeviceSim->write(0, sizeof(header), (const char *)&header);

	// TODO: put your format code here
	_freeBlockBitmap[0] = 1; // The first block is the header
	_freeBlockBitmap[1] = 1; // Second block is the bitmap
	_freeBlockBitmap[2] = 1; // Third block is the root directory
	blockDeviceSim->write(sizeof(header), BLOCK_SIZE, _freeBlockBitmap);
}

void MyFs::create_file(std::string path_str, bool directory)
{
	throw std::runtime_error("not implemented");
}

std::string MyFs::get_content(std::string path_str)
{
	throw std::runtime_error("not implemented");
	return "";
}

void MyFs::set_content(std::string path_str, std::string content)
{
	throw std::runtime_error("not implemented");
}

MyFs::dir_list MyFs::list_dir(std::string path_str)
{
	dir_list ans;
	throw std::runtime_error("not implemented");
	return ans;
}
