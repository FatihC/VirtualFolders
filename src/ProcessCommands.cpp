// This file is part of VFolders.
// Copyright 2025 by FatihCoskun.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "CommonData.h"
#include "model/VData.h"
#include <filesystem>
#include <vector>
#include <iostream>
#include <windows.h>
#include <winioctl.h>
#include "ProcessCommands.h"
#include "model/Session.h"



std::vector<VFile> listOpenFiles();


std::vector<VFile> listOpenFiles() {
    TCHAR configDir[MAX_PATH];
    ::SendMessage(plugin.nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)configDir);

    // Session files are typically stored in the same location as plugin config
    // but in a different subdirectory or as session.xml
    std::wstring sessionPath = std::wstring(configDir);
    // Remove "plugins\config" and add session info
    sessionPath = sessionPath.substr(0, sessionPath.find(L"\\plugins\\Config"));
    sessionPath += L"\\session.xml"; // This is where Notepad++ stores session info

    Session session = loadSessionFromXMLFile(sessionPath);

    std::vector<VFile> fileList;
    
    size_t i = 0;
    // Convert SessionFile objects to VFile objects
    // Process main view files
    for (i = 0; i < session.mainView.files.size(); ++i) {
        const auto& sessionFile = session.mainView.files[i];
        optional<VFile> vFileOpt = sessionFileToVFile(sessionFile, 0); // Main view
		if (!vFileOpt) continue;

        vFileOpt.value().setOrder(i);
        //vFile.isActive = session.mainView.activeIndex == vFile.docOrder;
        fileList.push_back(vFileOpt.value());
    }
    
    // Process sub view files
    for (size_t j = 0; j < session.subView.files.size(); ++j) {
        const auto& sessionFile = session.subView.files[j];
        optional<VFile> vFileOpt = sessionFileToVFile(sessionFile, 1); // Sub view
        if (!vFileOpt) continue;

        vFileOpt.value().setOrder(i + j);
        //vFile.isActive = session.subView.activeIndex == vFile.docOrder;
        fileList.push_back(vFileOpt.value());
    }
    
    return fileList;
}

optional<VFile> sessionFileToVFile(const SessionFile& sessionFile, int view) {
    VFile vFile;
    vFile.setOrder(0); // Will be set by the caller
    
    // If backupFilePath is empty, filename contains the absolute path
    // Extract just the filename from the path
    if (sessionFile.backupFilePath.empty()) {
        std::filesystem::path filePath(sessionFile.filename);
        if (!std::filesystem::exists(filePath)) {
            return nullopt;
        }
        vFile.name = filePath.filename().string();
        vFile.path = sessionFile.filename; // Keep original path
        vFile.isEdited = false;
    } else {
        vFile.name = sessionFile.filename;
        vFile.path = sessionFile.backupFilePath;
        vFile.isEdited = true;
    }

    string fileName = vFile.name;
    if (fileName.find_last_of("/\\") != string::npos) {
        size_t lastSlash = fileName.find_last_of("/\\");
        fileName = fileName.substr(lastSlash + 1);
		vFile.name = fileName;
    }
    
    vFile.view = view; // Use the passed view parameter
    vFile.session = 0; // Default session index
    vFile.backupFilePath = sessionFile.backupFilePath;
	vFile.isReadOnly = sessionFile.userReadOnly;

    return vFile;
    return const_cast<VFile&>(vFile);
}






