#include "VData.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <memory>  // for std::construct_at


using std::vector;
using std::map;
using std::optional;
using std::string;



vector<VFile*> VFolder::getAllFiles() const {
	vector<VFile*> allFiles;
	
	// Add all files from this folder
	for (const auto& file : fileList) {
		allFiles.push_back(const_cast<VFile*>(&file));
	}
	
	// Recursively add files from all subfolders
	for (const auto& subFolder : folderList) {
		vector<VFile*> subFolderFiles = subFolder.getAllFiles();
		allFiles.insert(allFiles.end(), subFolderFiles.begin(), subFolderFiles.end());
	}
	
	return allFiles;
}

int VFolder::getLastOrder() const {
	// Get the last order in this folder
	if (fileList.empty() && folderList.empty()) {
		return getOrder();
	}

	int maxFileOrder = fileList.empty() ? 0 :
		std::max_element(fileList.begin(), fileList.end(),
			[](const VBase& a, const VBase& b) {
				return a.getOrder() < b.getOrder();
			})->getOrder();

	int maxFolderOrder = folderList.empty() ? 0 :
		std::max_element(folderList.begin(), folderList.end(),
			[](const VBase& a, const VBase& b) {
				return a.getOrder() < b.getOrder();
			})->getLastOrder();	// Recursive call to get last order in subfolder

	return std::max(maxFileOrder, maxFolderOrder);
}

void VFolder::addFile(VFile* vFile) {
	vFile->setOrder(getLastOrder() + 1); // Set the order to be after the last file in this folder
	// Add file to the folder's file list
	fileList.push_back(*vFile);
}

int VFolder::countItemsInFolder() const {
	int count = 1; // Count the folder itself
	count += fileList.size();

	for (const auto& subFolder : folderList) {
		count += subFolder.countItemsInFolder();
	}
	return count;
}

void VFolder::vFolderSort()
{
	// Sort files in the folder by order
	std::sort(fileList.begin(), fileList.end(), [](const VFile& a, const VFile& b) {
		return a.getOrder() < b.getOrder();
		});
	// Sort subfolders by order
	std::sort(folderList.begin(), folderList.end(), [](const VFolder& a, const VFolder& b) {
		return a.getOrder() < b.getOrder();
		});
	// Recursively sort subfolders
	for (auto& subFolder : folderList) {
		subFolder.vFolderSort();
	}
}

VFile* VFolder::findFileByPath(const string& path, int view) const {
	for (const auto& file : fileList) {
		if (file.path == path && file.view == view) {
			return const_cast<VFile*>(&file); // Return a non-const pointer
		}
	}
	// If not found in root, search in folders
	for (const auto& folder : folderList) {
		VFile* foundFile = folder.findFileByPath(path, view);
		if (foundFile) {
			return foundFile;
		}
	}
	return nullptr; // Return null if not found
}

VFile* VFolder::findFileByName(const string& name, int view) const {
	for (const auto& file : fileList) {
		if (file.name == name && file.view == view) {
			return const_cast<VFile*>(&file); // Return a non-const pointer
		}
	}
	// If not found in root, search in folders
	for (const auto& folder : folderList) {
		VFile* foundFile = folder.findFileByName(name, view);
		if (foundFile) {
			return foundFile;
		}
	}
	return nullptr; // Return null if not found
}

optional<VBase*> VFolder::findAboveSibling(int order) {
	// Find the above sibling of the item with the given order
	optional<VBase*> aboveSibling = std::nullopt;
	// Check root-level files
	for (const auto& file : fileList) {
		if (file.getOrder() < order) {
			if (!aboveSibling || file.getOrder() > aboveSibling.value()->getOrder()) {
				aboveSibling = &const_cast<VFile&>(file);
			}
		}
	}
	// Check root-level folders
	for (const auto& folder : folderList) {
		if (folder.getOrder() < order) {
			if (!aboveSibling || folder.getOrder() > aboveSibling.value()->getOrder()) {
				aboveSibling = &const_cast<VFolder&>(folder);
			}
		}
	}
	return aboveSibling;
}

vector<VFile*> VFolder::getAllFilesByBufferID(UINT_PTR bufferID) const {
	vector<VFile*> foundedFiles;
	for (const auto& file : fileList) {
		if (file.bufferID == bufferID) {
			foundedFiles.push_back(const_cast<VFile*>(&file));
		}
	}

	// If not found in root, search in folders
	for (const auto& folder : folderList) {
		vector<VFile*> foundedFilesFromFolder = folder.getAllFilesByBufferID(bufferID);
		foundedFiles.insert(foundedFiles.end(), foundedFilesFromFolder.begin(), foundedFilesFromFolder.end());

	}

	return foundedFiles;
}


optional<VFile*> VFolder::findFileByOrder(int order) const {
	for (const auto& file : fileList) {
		if (file.getOrder() == order) {
			return &const_cast<VFile&>(file);
		}
	}

	// Recursively search in subfolders
	for (const auto& subFolder : folderList) {
		optional<VFile*> foundFile = subFolder.findFileByOrder(order);
		if (foundFile) {
			return foundFile;
		}
	}

	return std::nullopt; // Return null if not found  
}

optional<VFile*> VFolder::findFileByBufferID(UINT_PTR bufferID) const {
	for (const auto& file : fileList) {
		if (file.bufferID == bufferID) {
			return &const_cast<VFile&>(file);
		}
	}

	// If not found in root, search in folders
	for (const auto& folder : folderList) {
		optional<VFile*> foundFile = folder.findFileByBufferID(bufferID);
		if (foundFile) {
			return foundFile;
		}
	}

	return std::nullopt; // Return null if not found  
}

optional<VFile*> VFolder::findFileByBufferID(UINT_PTR bufferID, int view) const {
	for (const auto& file : fileList) {
		if (file.bufferID == bufferID && file.view == view) {
			return &const_cast<VFile&>(file);
		}
	}

	// If not found in root, search in folders
	for (const auto& folder : folderList) {
		optional<VFile*> foundFile = folder.findFileByBufferID(bufferID, view);
		if (foundFile) {
			return foundFile;
		}
	}

	return std::nullopt; // Return null if not found  
}

optional<VBase*> VFolder::getChildByOrder(int order) const {
	for (const auto& file : fileList) {
		if (file.getOrder() == order) {
			return &const_cast<VFile&>(file);
		}
	}
	for (const auto& folder : folderList) {
		if (folder.getOrder() == order) {
			return &const_cast<VFolder&>(folder);
		}
		else {
			optional<VBase*> child = folder.getChildByOrder(order);
			if (child) {
				return child;
			}
		}
	}
	return std::nullopt; // Return null if not found
}

optional<VFolder*> VFolder::findFolderByOrder(int order) const {
	for (const auto& folder : folderList) {
		if (folder.getOrder() == order) {
			return &const_cast<VFolder&>(folder);
		}
	}

	// Recursively search in subfolders
	for (const auto& subFolder : folderList) {
		optional<VFolder*> foundFolder = subFolder.findFolderByOrder(order);
		if (foundFolder) {
			return foundFolder;
		}
	}

	return std::nullopt; // Return null if not found
}

void VFolder::move(int steps) {
	// Adjust the order of this folder
	setOrder(getOrder() + steps); // Update the order and tree item
	for (auto& folder: folderList) {
		// Adjust the order of subfolders
		folder.move(steps);
	}

	for (auto& file : fileList) {
		// Adjust the order of files in this folder
		file.setOrder(file.getOrder() + steps);
	}

	// Sort files and subfolders after moving
	vFolderSort();
}

bool VFolder::isInRoot(int order) const {
	for (const auto& file : fileList) {
		if (file.getOrder() == order) {
			return true; // Found in root files
		}
	}

	for (const auto& folder : folderList) {
		if (folder.getOrder() == order) {
			return true; // Found in root folders
		}
	}
	return false;
}

VFolder* VFolder::findParentFolder(int order) const {
	// Recursively search in all root folders
	for (const auto& folder : folderList) {
		if (folder.getOrder() == order) {
			return const_cast<VFolder*>(this); // Found the parent folder
		}

		// Check files in current folder
		for (const auto& file : folder.fileList) {
			if (file.getOrder() == order) {
				return const_cast<VFolder*>(&folder); // Found the parent folder
			}
		}

		// Recursively search subfolders
		VFolder* foundInSubfolder = folder.findParentFolder(order);
		if (foundInSubfolder) {
			return foundInSubfolder;
		}
	}

	return nullptr; // Not found in any folder
}

void VFolder::removeFile(int order) {
	// Remove file by order
	fileList.erase(std::remove_if(fileList.begin(), fileList.end(),
		[order](const VFile& file) { return file.getOrder() == order; }), fileList.end());
}
void VFolder::removeChild(int order) {
	// Remove file by order
	fileList.erase(std::remove_if(fileList.begin(), fileList.end(),
		[order](const VFile& file) { return file.getOrder() == order; }), fileList.end());
	folderList.erase(std::remove_if(folderList.begin(), folderList.end(),
		[order](const VFolder& folder) { return folder.getOrder() == order; }), folderList.end());
}

vector<VBase*> VFolder::getAllChildren() {
	vector<VBase*> allChildren;
	// Add all root-level files
	for (const auto& file : fileList) {
		allChildren.push_back(const_cast<VFile*>(&file));
	}
	// Add all root-level folders
	for (const auto& folder : folderList) {
		allChildren.push_back(const_cast<VFolder*>(&folder));
	}

	// sort this vector by order
	std::sort(allChildren.begin(), allChildren.end(), [](const VBase* a, const VBase* b) {
		return a->getOrder() < b->getOrder();
		});

	return allChildren;
}

void VFolder::addChildren(vector<VBase*>& allChildren) {
	for (const auto& base : allChildren) {
		if (auto file = dynamic_cast<VFile*>(base)) {
			fileList.push_back(*file); // makes a copy
		}
		else if (auto folder = dynamic_cast<VFolder*>(base)) {
			folderList.push_back(*folder); // makes a copy
		}
	}
}

void VFolder::adjustOrders(int beginOrder, int endOrder, int step) {
	// Adjust orders of all files and folders in the range [beginOrder, endOrder]
	for (auto& file : fileList) {
		if (file.getOrder() >= beginOrder && file.getOrder() <= endOrder) {
			file.setOrder(file.getOrder() + step);
		}
	}
	for (auto& folder : folderList) {
		if (folder.getOrder() >= beginOrder && folder.getOrder() <= endOrder) {
			folder.setOrder(folder.getOrder() + step);
		}
		folder.adjustOrders(beginOrder, endOrder, step);
	}
}

json loadVDataFromFile(const std::wstring& filePath) {
    std::ifstream file(filePath);
    
    if (!file.is_open()) {
        return json::object();
    }
    
    // Check if file is empty
    file.seekg(0, std::ios::end);
    if (file.tellg() == 0) {
        return json::object();
    }
    
    // Reset file pointer and try to parse
    file.seekg(0, std::ios::beg);
    try {
        json vDataJson;
        file >> vDataJson;
        return vDataJson;
    } catch (const json::parse_error& e) {
        return json::object();
    }
}
