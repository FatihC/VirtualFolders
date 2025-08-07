#pragma once
#include <string>
#include <vector>
#include <optional>
#include <fstream>
#include <map>
#include <set>
#include <algorithm>
#include "nlohmann/json.hpp"
#include <windows.h>
#include "DateUtil.h"

using json = nlohmann::json;

using std::string;
using std::vector;

class VFile
{
public:
	int order;
	string name;
	string path;
	int view;
	int session;
	string backupFilePath;
	bool isActive = false;
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
	
	// Returns a vector of all VFile objects in this folder and all subfolders
	vector<VFile> getAllFiles() const;
};

class VData
{
public:
	vector<VFolder> folderList;
	vector<VFile> fileList;

	vector<VFile> getAllFiles() const;
};

// JSON serialization functions (must remain inline for nlohmann/json)
inline void to_json(json& j, const VFile& f) {
	j = json{ 
		{"order", f.order},
		{"name", f.name}, 
		{"path", f.path},
		{"view", f.view},
		{"session", f.session},
		{"backupFilePath", f.backupFilePath},
		{"isActive", f.isActive}
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

inline void from_json(const json& j, VFile& f) {
	j.at("order").get_to(f.order);
	j.at("name").get_to(f.name);
	j.at("path").get_to(f.path);
	j.at("view").get_to(f.view);

	if (j.contains("session")) j.at("session").get_to(f.session);
	if (j.contains("backupFilePath")) j.at("backupFilePath").get_to(f.backupFilePath);
	if (j.contains("isActive")) j.at("isActive").get_to(f.isActive);
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
	// Check if JSON is null or empty
	if (j.is_null() || j.empty()) {
		data.folderList.clear();
		data.fileList.clear();
		return;
	}
	
	if (j.contains("folderList")) {
		j.at("folderList").get_to(data.folderList);
	} else {
		data.folderList.clear();
	}
	
	// Only set fileList if it exists in the JSON
	if (j.contains("fileList")) {
		j.at("fileList").get_to(data.fileList);
	} else {
		data.fileList.clear();
	}
}

// Function declarations for functions implemented in VData.cpp
void vFolderSort(VFolder& vFolder);
void vDataSort(VData& vData);
std::optional<VFile> findFileByOrder(const vector<VFile>& fileList, int order);
std::optional<VFolder> findFolderByOrder(const vector<VFolder>& folderList, int order);
json loadVDataFromFile(const std::wstring& filePath);
void syncVDataWithOpenFiles(VData& vData, const std::vector<VFile>& openFiles);
