#pragma once
#include <string>
#include <vector>
#include "nlohmann/json.hpp"
using json = nlohmann::json;


using namespace std;

class VFile
{
public:
	string name;
	string path;
};

class VFolder
{
public:
	vector<VFile> fileList;
};

class VData
{
public:
	vector<VFolder> folderList;
	vector<VFile> fileList; // Optional: if you want to keep a list of files at the root level
};

void to_json(json& j, const VFile& f) {
	j = json{ {"name", f.name}, {"path", f.path} };
}

void to_json(json& j, const VFolder& folder) {
	j = json{ {"fileList", folder.fileList} };
}

void to_json(json& j, const VData& data) {
	j = json{ 
		{"folderList", data.folderList},
		{"fileList", data.fileList}
	};
}