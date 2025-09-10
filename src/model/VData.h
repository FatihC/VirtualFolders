#pragma once
#include <string>
#include <vector>
#include <optional>
#include <fstream>
#include <map>
#include <set>
#include <algorithm>
#include "nlohmann/json.hpp"
#define NOMINMAX
#include <windows.h>
#include "DateUtil.h"
#include <CommCtrl.h>


using json = nlohmann::json;

using std::string;
using std::vector;
using std::optional;


class VBase {
protected:
	int order;

public:
	string name;
	string path;
	HTREEITEM hTreeItem = nullptr; // Pointer to the tree item in the watcher panel

	friend void updateTreeItemLParam(VBase* vBase);

	virtual ~VBase() = default; // Make the class polymorphic

	void setOrder(int newOrder) {
		order = newOrder;
		updateTreeItemLParam(this);
	}
	int getOrder() const { return order; }

	void incrementOrder() { ++order; updateTreeItemLParam(this); }
	void decrementOrder() { --order; updateTreeItemLParam(this); }
};


class VFile : public VBase
{
public:
	// Add this inside the VFile class definition, after the private section
	friend void from_json(const json& j, VFile& f);

	UINT_PTR bufferID = 0;
	
	int view;
	int session;
	string backupFilePath;
	bool isActive = false;
	bool isEdited = false;
	bool isReadOnly = false;

};

class VFolder : public VBase
{
public:
	// Add this inside the VFile class definition, after the private section
	friend void from_json(const json& j, VFolder& f);


	bool isExpanded = false;
	vector<VFolder> folderList;
	vector<VFile> fileList;

	
	// Returns a vector of all VFile objects in this folder and all subfolders
	vector<VFile*> getAllFiles() const;
	void vFolderSort();
	optional<VFile*> findFileByOrder(int order) const;
	optional<VFile*> findFileByBufferID(UINT_PTR bufferID) const;
	optional<VFile*> findFileByBufferID(UINT_PTR bufferID, int view) const;
	optional<VFolder*> findFolderByOrder(int order) const;
	void move(int steps);
	VFolder* findParentFolder(int order) const;
	void removeFile(int order);
	void removeChild(int order);
	void adjustOrders(int beginOrder, int endOrder, int step);
	void addFile(VFile* vFile);
	int getLastOrder() const;
	VFile* findFileByPath(const string& path) const;
	VFile* findFileByName(const string& name) const;
	int countItemsInFolder() const;
	optional<VBase*> getChildByOrder(int order) const;
	optional<VBase*> findAboveSibling(int order);
	vector<VBase*> getAllChildren();
	void addChildren(vector<VBase*>& allChildren);
	vector<VFile*> getAllFilesByBufferID(UINT_PTR bufferID) const;
};

class VData
{
public:
	vector<VFolder> folderList;
	vector<VFile> fileList;

	vector<VFile*> getAllFiles() const;
	void vDataSort();
	optional<VFile*> findFileByOrder(int order) const;
	optional<VFolder*> findFolderByOrder(int order) const;
	optional<VFile*> findFileByBufferID(UINT_PTR bufferID) const;
	optional<VFile*> findFileByBufferID(UINT_PTR bufferID, int view) const;
	bool isInRoot(int order) const;
	VFolder* findParentFolder(int order) const;
	void adjustOrders(int beginOrder, int endOrder, int step);
	void removeFile(int order);
	void removeChild(int order);
	VFile* findFileByPath(const string& path) const;
	VFile* findFileByName(const string& name) const;
	int getLastOrder() const;
	optional<VBase*> findAboveSibling(int order);
	vector<VBase*> getAllChildren();
	void addChildren(vector<VBase*>& allChildren);
	vector<VFile*> getAllFilesByBufferID(UINT_PTR bufferID) const;
};

// JSON serialization functions (must remain inline for nlohmann/json)
inline void to_json(json& j, const VFile& f) {
	j = json{ 
		{"order", f.getOrder()},
		{"name", f.name}, 
		{"path", f.path},
		{"view", f.view},
		{"session", f.session},
		{"backupFilePath", f.backupFilePath},
		{"isActive", f.isActive},
		{"isEdited", f.isEdited},
		{"isReadOnly", f.isReadOnly}
	};
}

inline void to_json(json& j, const VFolder& folder) {
	j = json{ 
		{"order", folder.getOrder()},
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
	if (j.contains("order")) j.at("order").get_to(f.order);
	if (j.contains("name")) j.at("name").get_to(f.name);
	if (j.contains("path")) j.at("path").get_to(f.path);
	if (j.contains("view")) j.at("view").get_to(f.view);

	if (j.contains("session")) j.at("session").get_to(f.session);
	if (j.contains("backupFilePath")) j.at("backupFilePath").get_to(f.backupFilePath);
	if (j.contains("isActive")) j.at("isActive").get_to(f.isActive);
	if (j.contains("isEdited")) j.at("isEdited").get_to(f.isEdited);
	if (j.contains("isReadOnly")) j.at("isReadOnly").get_to(f.isReadOnly);
}

inline void from_json(const json& j, VFolder& folder) {
	if (j.contains("order")) j.at("order").get_to(folder.order);
	if (j.contains("name")) j.at("name").get_to(folder.name);
	if (j.contains("path")) j.at("path").get_to(folder.path);
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
json loadVDataFromFile(const std::wstring& filePath);
void syncVDataWithOpenFiles(VData& vData, std::vector<VFile>& openFiles);
