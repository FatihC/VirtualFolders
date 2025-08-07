#include "VData.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <set>

vector<VFile> VFolder::getAllFiles() const {
	vector<VFile> allFiles;
	
	// Add all files from this folder
	allFiles.insert(allFiles.end(), fileList.begin(), fileList.end());
	
	// Recursively add files from all subfolders
	for (const auto& subFolder : folderList) {
		vector<VFile> subFolderFiles = subFolder.getAllFiles();
		allFiles.insert(allFiles.end(), subFolderFiles.begin(), subFolderFiles.end());
	}
	
	return allFiles;
}

vector<VFile> VData::getAllFiles() const {
	vector<VFile> allFiles;
	
	// Add all files from this folder
	allFiles.insert(allFiles.end(), fileList.begin(), fileList.end());
	
	// Recursively add files from all subfolders
	for (const auto& subFolder : folderList) {
		vector<VFile> subFolderFiles = subFolder.getAllFiles();
		allFiles.insert(allFiles.end(), subFolderFiles.begin(), subFolderFiles.end());
	}
	
	return allFiles;
}

void vFolderSort(VFolder& vFolder)
{
	// Sort files in the folder by order
	std::sort(vFolder.fileList.begin(), vFolder.fileList.end(), [](const VFile& a, const VFile& b) {
		return a.order < b.order;
		});
	// Sort subfolders by order
	std::sort(vFolder.folderList.begin(), vFolder.folderList.end(), [](const VFolder& a, const VFolder& b) {
		return a.order < b.order;
		});
	// Recursively sort subfolders
	for (auto& subFolder : vFolder.folderList) {
		vFolderSort(subFolder);
	}
}

void vDataSort(VData& vData) {
	// Sort folders by order
	std::sort(vData.folderList.begin(), vData.folderList.end(), [](const VFolder& a, const VFolder& b) {
		return a.order < b.order;
		});
	for (auto& folder : vData.folderList) {
		vFolderSort(folder);
	}

	// Optionally sort root-level files if you have any
	std::sort(vData.fileList.begin(), vData.fileList.end(), [](const VFile& a, const VFile& b) {
		return a.order < b.order;
		});
}

std::optional<VFile> findFileByOrder(const vector<VFile>& fileList, int order) {
	for (const auto& file : fileList) {
		if (file.order == order) {
			return file;
		}
	}
	return std::nullopt; // Return null if not found
}

std::optional<VFolder> findFolderByOrder(const vector<VFolder>& folderList, int order) {
	for (const auto& folder : folderList) {
		if (folder.order == order) {
			return folder;
		}
	}
	return std::nullopt; // Return null if not found
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

void syncVDataWithOpenFiles(VData& vData, const std::vector<VFile>& openFiles) {
    // Create a map of existing files in vData for quick lookup using name and backupFilePath
    std::map<std::string, size_t> existingFileIndices;
	vector<VFile> allFilesOfVData = vData.getAllFiles();
	
	// Populate the map with existing files
	for (size_t i = 0; i < allFilesOfVData.size(); ++i) {
		std::string key = allFilesOfVData[i].name + "|" + allFilesOfVData[i].backupFilePath;
		existingFileIndices[key] = i;
	}
    
    // Track which open files we've processed
    std::set<std::string> processedOpenFiles;
    
    // Check each open file
    for (const auto& openFile : openFiles) {
        std::string key = openFile.name + "|" + openFile.backupFilePath;
        processedOpenFiles.insert(key);
        
        // Check if this file exists in vData
        auto it = existingFileIndices.find(key);
		if (it != existingFileIndices.end()) {
			size_t index = it->second;
			if (index < allFilesOfVData.size()) {
				VFile& existingFile = allFilesOfVData[index];
				if (existingFile.backupFilePath != openFile.backupFilePath) {
					existingFile = openFile;
				}
				else {
					existingFile.isActive = openFile.isActive;
				}
			}
		}
		else {
			vData.fileList.push_back(openFile);
		}
    }
    
    // Remove files from vData that are no longer in openFiles
    vData.fileList.erase(
        std::remove_if(vData.fileList.begin(), vData.fileList.end(),
            [&processedOpenFiles](const VFile& file) {
                std::string key = file.name + "|" + file.backupFilePath;
                return processedOpenFiles.find(key) == processedOpenFiles.end();
            }),
        vData.fileList.end()
    );
}
