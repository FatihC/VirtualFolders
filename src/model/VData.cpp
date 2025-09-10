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

vector<VFile*> VData::getAllFiles() const {
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
			})->getLastOrder();

	return std::max(maxFileOrder, maxFolderOrder);
}

int VData::getLastOrder() const {
	// Get the last order in this folder
	if (fileList.empty() && folderList.empty()) {
		return 0;
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
			})->getLastOrder();

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

VFile* VData::findFileByPath(const string& path) const {
	for (const auto& file : fileList) {
		if (file.path == path) {
			return const_cast<VFile*>(&file); // Return a non-const pointer
		}
	}
	// If not found in root, search in folders
	for (const auto& folder : folderList) {
		VFile* foundFile = folder.findFileByPath(path);
		if (foundFile) {
			return foundFile;
		}
	}
	return nullptr; // Return null if not found
}

VFile* VData::findFileByName(const string& name) const {
	for (const auto& file : fileList) {
		if (file.name == name) {
			return const_cast<VFile*>(&file); // Return a non-const pointer
		}
	}
	// If not found in root, search in folders
	for (const auto& folder : folderList) {
		VFile* foundFile = folder.findFileByName(name);
		if (foundFile) {
			return foundFile;
		}
	}
	return nullptr; // Return null if not found
}

VFile* VFolder::findFileByPath(const string& path) const {
	for (const auto& file : fileList) {
		if (file.path == path) {
			return const_cast<VFile*>(&file); // Return a non-const pointer
		}
	}
	// If not found in root, search in folders
	for (const auto& folder : folderList) {
		VFile* foundFile = folder.findFileByPath(path);
		if (foundFile) {
			return foundFile;
		}
	}
	return nullptr; // Return null if not found
}

VFile* VFolder::findFileByName(const string& name) const {
	for (const auto& file : fileList) {
		if (file.name == name) {
			return const_cast<VFile*>(&file); // Return a non-const pointer
		}
	}
	// If not found in root, search in folders
	for (const auto& folder : folderList) {
		VFile* foundFile = folder.findFileByName(name);
		if (foundFile) {
			return foundFile;
		}
	}
	return nullptr; // Return null if not found
}

void VData::vDataSort() {
	// Sort folders by order
	std::sort(folderList.begin(), folderList.end(), [](const VFolder& a, const VFolder& b) {
		return a.getOrder() < b.getOrder();
		});
	for (auto& folder : folderList) {
		folder.vFolderSort();
	}

	// Optionally sort root-level files if you have any
	std::sort(fileList.begin(), fileList.end(), [](const VFile& a, const VFile& b) {
		return a.getOrder() < b.getOrder();
		});
}

optional<VFile*> VData::findFileByOrder(int order) const {  
    for (const auto& file : fileList) {  
        if (file.getOrder() == order) {
            return &const_cast<VFile&>(file);  
        }  
    }

	// If not found in root, search in folders
	for (const auto& folder : folderList) {
		optional<VFile*> foundFile = folder.findFileByOrder(order);
		if (foundFile) {
			return foundFile;
		}
	}

    return std::nullopt; // Return null if not found  
}

optional<VBase*> VData::findAboveSibling(int order) {
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

optional<VFile*> VData::findFileByBufferID(UINT_PTR bufferID) const {
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

optional<VFile*> VData::findFileByBufferID(UINT_PTR bufferID, int view) const {
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

vector<VFile*> VData::getAllFilesByBufferID(UINT_PTR bufferID) const {
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
	}
	return std::nullopt; // Return null if not found
}

optional<VFolder*> VData::findFolderByOrder(int order) const {
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

bool VData::isInRoot(int order) const {
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

VFolder* VData::findParentFolder(int order) const {
	// Recursively search in all root folders
	for (const auto& folder : folderList) {
		// Check files in current folder
		for (const auto& file : folder.fileList) {
			if (file.getOrder() == order) {
				return const_cast<VFolder*>(&folder); // Found the parent folder
			}
		}

		// Check folders in current folder
		for (const auto& subFolder : folder.folderList) {
			if (subFolder.getOrder() == order) {
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

VFolder* VFolder::findParentFolder(int order) const {
	// Recursively search in all root folders
	for (const auto& folder : folderList) {
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


void VData::removeFile(int order) {
	// Remove file by order
	fileList.erase(std::remove_if(fileList.begin(), fileList.end(),
		[order](const VFile& file) { return file.getOrder() == order; }), fileList.end());
}

void VData::removeChild(int order) {
	// Remove file by order
	fileList.erase(std::remove_if(fileList.begin(), fileList.end(),
		[order](const VFile& file) { return file.getOrder() == order; }), fileList.end());
	folderList.erase(std::remove_if(folderList.begin(), folderList.end(),
		[order](const VFolder& folder) { return folder.getOrder() == order; }), folderList.end());
}

vector<VBase*> VData::getAllChildren() {
	vector<VBase*> allChildren;
	// Add all root-level files
	for (const auto& file : fileList) {
		allChildren.push_back(const_cast<VFile*>(&file));
	}
	// Add all root-level folders
	for (const auto& folder : folderList) {
		allChildren.push_back(const_cast<VFolder*>(&folder));
	}
	return allChildren;
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

void VData::addChildren(vector<VBase*>& allChildren) {
	for (const auto& base : allChildren) {
		if (auto file = dynamic_cast<VFile*>(base)) {
			fileList.push_back(*file); // makes a copy
		}
		else if (auto folder = dynamic_cast<VFolder*>(base)) {
			folderList.push_back(*folder); // makes a copy
		}
	}
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

void VData::adjustOrders(int beginOrder, int endOrder, int step) {
	if (!endOrder) {
		endOrder = INT_MAX; // If endOrder is not specified, adjust all orders
	}
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



void syncVDataWithOpenFiles(VData& vData, vector<VFile>& openFiles) {
    // Create a map of existing files in vData for quick lookup using name and backupFilePath
    map<string, size_t> existingFileIndices;
	vector<VFile*> allFilesOfVData = vData.getAllFiles();
	
	// Populate the map with existing files
	for (size_t i = 0; i < allFilesOfVData.size(); ++i) {
		string key = allFilesOfVData[i]->path;
		existingFileIndices[key] = i;
	}
    
    // Track which open files we've processed
    std::set<string> processedOpenFiles;
    
    // Check each open file
	for (int i = 0; i < openFiles.size(); i++) {
    //for (const auto& openFile : openFiles) {
        /*string key = openFiles[i].backupFilePath.length() == 0 ? openFiles[i] : "";*/
		string key = openFiles[i].path;
        processedOpenFiles.insert(key);
        
        // Check if this file exists in vData
        auto it = existingFileIndices.find(key);
		if (it != existingFileIndices.end()) {
			size_t index = it->second;
			if (index < allFilesOfVData.size()) {
				VFile* existingFile = allFilesOfVData[index];
				if (existingFile->backupFilePath != openFiles[i].backupFilePath) {
					existingFile = &openFiles[i];
				}
				else {
					existingFile->isActive = openFiles[i].isActive;
				}
				existingFile->isEdited = openFiles[i].isEdited;
				existingFile->view = openFiles[i].view;
			}
		}
		else {
			vData.fileList.push_back(openFiles[i]);
		}
    }
    
    // Remove files from vData that are no longer in openFiles
    vData.fileList.erase(
        std::remove_if(vData.fileList.begin(), vData.fileList.end(),
            [&processedOpenFiles](const VFile& file) {
                string key = file.path;
                return processedOpenFiles.find(key) == processedOpenFiles.end();
            }),
        vData.fileList.end()
    );
}

