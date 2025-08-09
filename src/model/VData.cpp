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

void VFolder::vFolderSort()
{
	// Sort files in the folder by order
	std::sort(fileList.begin(), fileList.end(), [](const VFile& a, const VFile& b) {
		return a.order < b.order;
		});
	// Sort subfolders by order
	std::sort(folderList.begin(), folderList.end(), [](const VFolder& a, const VFolder& b) {
		return a.order < b.order;
		});
	// Recursively sort subfolders
	for (auto& subFolder : folderList) {
		subFolder.vFolderSort();
	}
}

void VData::vDataSort() {
	// Sort folders by order
	std::sort(folderList.begin(), folderList.end(), [](const VFolder& a, const VFolder& b) {
		return a.order < b.order;
		});
	for (auto& folder : folderList) {
		folder.vFolderSort();
	}

	// Optionally sort root-level files if you have any
	std::sort(fileList.begin(), fileList.end(), [](const VFile& a, const VFile& b) {
		return a.order < b.order;
		});
}

optional<VFile*> VData::findFileByOrder(int order) const {  
    for (const auto& file : fileList) {  
        if (file.order == order) {  
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

optional<VFile*> VFolder::findFileByOrder(int order) const {
	for (const auto& file : fileList) {
		if (file.order == order) {
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

optional<VFolder*> VData::findFolderByOrder(int order) const {
	for (const auto& folder : folderList) {
		if (folder.order == order) {
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
		if (folder.order == order) {
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
	order += steps;
	for (auto& folder: folderList) {
		// Adjust the order of subfolders
		folder.move(steps);
	}

	for (auto& file : fileList) {
		// Adjust the order of files in this folder
		file.order += steps;
	}

	// Sort files and subfolders after moving
	vFolderSort();
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
					existingFile->docOrder = i;
				}
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
