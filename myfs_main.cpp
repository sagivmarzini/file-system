#include "blkdev.h"
#include "myfs.h"
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>

const std::string FS_NAME = "myfs";

const std::string LIST_CMD = "ls";
const std::string CONTENT_CMD = "cat";
const std::string CREATE_FILE_CMD = "touch";
const std::string CREATE_DIR_CMD = "mkdir";
const std::string EDIT_CMD = "edit";
const std::string TREE_CMD = "tree";
const std::string HELP_CMD = "help";
const std::string EXIT_CMD = "exit";
const std::string REMOVE_CMD = "rm";
const std::string MOVE_CMD = "mv";

const std::string HELP_STRING = "The following commands are supported: \n" +
								LIST_CMD + " [<directory>] - list directory content. \n" +
								CONTENT_CMD + " <path> - show file content. \n" +
								CREATE_FILE_CMD + " <path> - create empty file. \n" +
								CREATE_DIR_CMD + " <path> - create empty directory. \n" +
								EDIT_CMD + " <path> - re-set file content. \n" +
								REMOVE_CMD + " <path> - remove file or directory. \n" +
								MOVE_CMD + " <source> <destination> - move/rename file or directory. \n" +
								HELP_CMD + " - show this help message. \n" +
								EXIT_CMD + " - gracefully exit. \n";
std::vector<std::string> split_cmd(std::string cmd)
{
	std::stringstream ss(cmd);
	std::string part;
	std::vector<std::string> ans;

	while (std::getline(ss, part, ' '))
		ans.push_back(part);

	return ans;
}

static void recursive_print(MyFs &myfs, std::string path, std::string prefix = "")
{
	MyFs::INodeList dlist = myfs.list_dir(path);
	for (size_t i = 0; i < dlist.size(); i++)
	{
		MyFs::INodeEntry &curr_entry = dlist[i];

		std::string entry_prefix = prefix;
		if (i == dlist.size() - 1)
			entry_prefix += "└── ";
		else
			entry_prefix += "├── ";

		std::cout << entry_prefix << curr_entry.name << std::endl;

		if (curr_entry.isDir)
		{
			std::string dir_prefix = prefix;

			if (i == dlist.size() - 1)
				dir_prefix += "    ";
			else
				dir_prefix += "│   ";
			recursive_print(myfs, path + "/" + curr_entry.name, dir_prefix);
		}
	}
}

int main(int argc, char **argv)
{

	if (argc != 2)
	{
		std::cerr << "Please provide the file to operate on" << std::endl;
		return -1;
	}

	BlockDeviceSimulator *blockDevicePointer = new BlockDeviceSimulator(argv[1]);
	MyFs myfs(blockDevicePointer);
	bool exit = false;

	std::cout << "Welcome to " << FS_NAME << std::endl;
	std::cout << "To get help, please type 'help' on the prompt below." << std::endl;
	std::cout << std::endl;

	while (!exit)
	{
		try
		{
			std::string cmdline;
			std::cout << FS_NAME << "$ ";
			std::getline(std::cin, cmdline, '\n');
			if (cmdline == std::string(""))
				continue;

			std::vector<std::string> cmd = split_cmd(cmdline);

			if (cmd[0] == LIST_CMD)
			{
				MyFs::INodeList dlist;
				if (cmd.size() == 1)
					dlist = myfs.list_dir("/");
				else if (cmd.size() == 2)
					dlist = myfs.list_dir(cmd[1]);
				else
					std::cout << LIST_CMD << ": one or zero arguments requested" << std::endl;

				for (size_t i = 0; i < dlist.size(); i++)
				{
					std::cout << std::setw(15) << std::left
							  << std::string(dlist[i].name) + (dlist[i].isDir ? "/" : "")
							  << std::setw(10) << std::right
							  << dlist[i].fileSize << std::endl;
				}
			}
			else if (cmd[0] == EXIT_CMD)
			{
				exit = true;
			}
			else if (cmd[0] == HELP_CMD)
			{
				std::cout << HELP_STRING;
			}
			else if (cmd[0] == CREATE_FILE_CMD)
			{
				if (cmd.size() == 2)
					myfs.create_file(cmd[1], false);
				else
					std::cout << CREATE_FILE_CMD << ": file path requested" << std::endl;
			}
			else if (cmd[0] == CONTENT_CMD)
			{
				if (cmd.size() == 2)
					std::cout << myfs.get_content(cmd[1]) << std::endl;
				else
					std::cout << CONTENT_CMD << ": file path requested" << std::endl;
			}
			else if (cmd[0] == TREE_CMD)
			{
				recursive_print(myfs, "");
			}
			else if (cmd[0] == EDIT_CMD)
			{
				if (cmd.size() == 2)
				{
					std::cout << "Enter new file content" << std::endl;
					std::string content;
					std::string curr_line;
					std::getline(std::cin, curr_line);
					while (curr_line != "")
					{
						content += curr_line + "\n";
						std::getline(std::cin, curr_line);
					}
					myfs.set_content(cmd[1], content);
				}
				else
				{
					std::cout << EDIT_CMD << ": file path requested" << std::endl;
				}
			}
			else if (cmd[0] == CREATE_DIR_CMD)
			{
				if (cmd.size() == 2)
					myfs.create_file(cmd[1], true);
				else
					std::cout << CREATE_DIR_CMD << ": one argument requested" << std::endl;
			}
			else if (cmd[0] == REMOVE_CMD)
			{
				if (cmd.size() == 2)
					myfs.remove_file(cmd[1]);
				else
					std::cout << REMOVE_CMD << ": file or directory path requested" << std::endl;
			}
			else if (cmd[0] == MOVE_CMD)
			{
				if (cmd.size() == 3)
					myfs.rename_file(cmd[1], cmd[2]);
				else
					std::cout << MOVE_CMD << ": source and destination paths requested" << std::endl;
			}
			else
			{
				std::cout << "unknown command: " << cmd[0] << std::endl;
			}
		}
		catch (std::exception &e)
		{
			std::cout << e.what() << std::endl;
		}
	}
}
