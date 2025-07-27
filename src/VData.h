#pragma once
#include <string>
#include <vector>
#include <optional>
#include "nlohmann/json.hpp"
#include <windows.h>
#include "DateUtil.h"

using json = nlohmann::json;


using namespace std;

class VFile
{
public:
	int order;
	string name;
	string path;
	int view;
	wstring id;
    FILETIME creationTime;
};

class VFolder
{
public:
	int order;
	string name;
	string path;
	bool isExpanded = false;
	vector<VFolder> folderList;
	vector<VFile> fileList;
};

class VData
{
public:
	vector<VFolder> folderList;
	vector<VFile> fileList; // Optional: if you want to keep a list of files at the root level
};

inline void to_json(json& j, const VFile& f) {
	j = json{ 
		{"order", f.order},
		{"name", f.name}, 
		{"path", f.path},
		{"view", f.view},
		{"id", f.id},
		{"creationTime", filetimeToInteger(f.creationTime)}
	};
}

inline void to_json(json& j, const VFolder& folder) {
	j = json{ 
		{"order", folder.order},
		{"name", folder.name},
		{"path", folder.path},
		{"isExpanded", folder.isExpanded},
		{"folderList", folder.folderList},
		{"fileList", folder.fileList} 
	};
}

inline void to_json(json& j, const VData& data) {
	j = json{ 
		{"folderList", data.folderList},
		{"fileList", data.fileList}
	};
}

// Add these to VData.h (after your to_json functions)

inline void from_json(const json& j, VFile& f) {
	j.at("order").get_to(f.order);
	j.at("name").get_to(f.name);
	j.at("path").get_to(f.path);
	j.at("view").get_to(f.view);
	
	// Convert string to wstring for id
	string idStr;
	j.at("id").get_to(idStr);
	f.id = wstring(idStr.begin(), idStr.end());
	
	// Convert ULONGLONG to FILETIME for creationTime
	ULONGLONG timeValue;
	j.at("creationTime").get_to(timeValue);
	f.creationTime = ULongLongToFileTime(timeValue);
}

inline void from_json(const json& j, VFolder& folder) {
	j.at("order").get_to(folder.order);
	j.at("name").get_to(folder.name);
	j.at("path").get_to(folder.path);
	if (j.contains("isExpanded")) j.at("isExpanded").get_to(folder.isExpanded);
	if (j.contains("folderList")) j.at("folderList").get_to(folder.folderList);
	if (j.contains("fileList")) j.at("fileList").get_to(folder.fileList);
}

inline void from_json(const json& j, VData& data) {
	
	if (j.contains("folderList")) {
		j.at("folderList").get_to(data.folderList);
	}
	// Only set fileList if it exists in the JSON
	if (j.contains("fileList")) {
		j.at("fileList").get_to(data.fileList);
	}
}


inline void vFolderSort(VFolder& vFolder)
{
	// Sort files in the folder by order
	std::sort(vFolder.fileList.begin(), vFolder.fileList.end(), [](const VFile& a, const VFile& b) {
		return a.order > b.order;
		});
	// Sort subfolders by order
	std::sort(vFolder.folderList.begin(), vFolder.folderList.end(), [](const VFolder& a, const VFolder& b) {
		return a.order > b.order;
		});
	// Recursively sort subfolders
	for (auto& subFolder : vFolder.folderList) {
		vFolderSort(subFolder);
	}
}

inline void vDataSort(VData vData) {
	// Sort folders by order
	std::sort(vData.folderList.begin(), vData.folderList.end(), [](const VFolder& a, const VFolder& b) {
		return a.order > b.order;
		});
	for (auto& folder : vData.folderList) {
		vFolderSort(folder);
	}

	// Optionally sort root-level files if you have any
	std::sort(vData.fileList.begin(), vData.fileList.end(), [](const VFile& a, const VFile& b) {
		return a.order > b.order;
		});
}

inline std::optional<VFile> findFileByOrder(vector<VFile> fileList, int order) {
	for (const auto& file : fileList) {
		if (file.order == order) {
			return file;
		}
	}
	return std::nullopt; // Return null if not found
}

inline std::optional<VFolder> findFolderByOrder(vector<VFolder> folderList, int order) {
	for (const auto& folder : folderList) {
		if (folder.order == order) {
			return folder;
		}
	}
	return std::nullopt; // Return null if not found
}
