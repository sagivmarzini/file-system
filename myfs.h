#ifndef __MYFS_H__
#define __MYFS_H__

#include <memory>
#include <vector>
#include <stdint.h>
#include "blkdev.h"

#define FILE_NAME_LEN 10

#define BLOCK_SIZE sizeof(MyFs::dir_list_entry)

class MyFs
{
public:
	MyFs(BlockDeviceSimulator *blkdevsim_);

	/**
	 * dir_list_entry struct
	 * This struct is used by list_dir method to return directory entry
	 * information.
	 */
	struct INodeEntry
	{
		// Index of the INode
		int index;

		// The directory entry name
		// std::string name;
		char name[FILE_NAME_LEN];

		// Whether the entry is a file or a directory
		bool isDir;

		// File size
		int fileSize;

		// Where the content is located
		int contentAddress;
	};
	typedef std::vector<struct INodeEntry> INodeList;

	/**
	 * format method
	 * This function discards the current content in the blockdevice and
	 * create a fresh new MYFS instance in the blockdevice.
	 */
	void format();

	/**
	 * create_file method
	 * Creates a new file in the required path.
	 * @param path_str the file path (e.g. "/newfile")
	 * @param isDiretory boolean indicating whether this is a file or directory
	 */
	void create_file(const std::string &path_str, bool isDiretory);

	/**
	 * get_content method
	 * Returns the whole content of the file indicated by path_str param.
	 * Note: this method assumes path_str refers to a file and not a
	 * directory.
	 * @param path_str the file path (e.g. "/somefile")
	 * @return the content of the file
	 */
	std::string get_content(const std::string &path_str);

	/**
	 * set_content method
	 * Sets the whole content of the file indicated by path_str param.
	 * Note: this method assumes path_str refers to a file and not a
	 * directory.
	 * @param path_str the file path (e.g. "/somefile")
	 * @param content the file content string
	 */
	void set_content(const std::string &path_str, const std::string &content);

	/**
	 * list_dir method
	 * Returns a list of a files in a directory.
	 * Note: this method assumes path_str refers to a directory and not a
	 * file.
	 * @param path_str the file path (e.g. "/somedir")
	 * @return a vector of dir_list_entry structures, one for each file in
	 *	the directory.
	 */
	INodeList list_dir(const std::string &path_str);

	/**
	 * This struct represents the first bytes of a myfs filesystem.
	 * It holds some magic characters and a number indicating the version.
	 * Upon class construction, the magic and the header are tested - if
	 * they both exist than the file is assumed to contain a valid myfs
	 * instance. Otherwise, the blockdevice is formated and a new instance is
	 * created.
	 */
	struct myfs_header
	{
		char magic[4];
		uint8_t version;
	};

private:
	BlockDeviceSimulator *_blockDeviceSim;

	static const uint8_t CURR_VERSION = 0x03;
	static const char *MYFS_MAGIC;

	int _lastINodeIndex;
	int _lastFileAddress;

	void updateIndexTable() const;
	void readIndexTable();
	INodeEntry getINodeAtIndex(const int index) const;
	INodeEntry getINodeByName(const std::string &path_str) const;
};

#endif // __MYFS_H__
