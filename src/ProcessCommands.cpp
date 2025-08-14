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


void printListOpenFiles() {
    Scintilla::EndOfLine eolMode = sci.EOLMode();
    std::wstring eol = eolMode == Scintilla::EndOfLine::Cr ? L"\r"
        : eolMode == Scintilla::EndOfLine::Lf ? L"\n"
        : L"\r\n";
    std::wstring filenames = commonData.heading.get() + eol;
    for (int view = 0; view < 2; ++view) {
        if (npp(NPPM_GETCURRENTDOCINDEX, 0, view)) {
            size_t n = npp(NPPM_GETNBOPENFILES, 0, view + 1);
            for (size_t i = 0; i < n; ++i) {
                std::wstring filepath = getFilePath(npp(NPPM_GETBUFFERIDFROMPOS, i, view));
                if (!filepath.empty()) {
                    filenames += filepath + eol;
                }
            }
        }
    }
    sci.InsertText(-1, fromWide(filenames).data());
	//listOpenFiles(); // Call the function to list open files
}

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
    
    // Convert SessionFile objects to VFile objects
    // Process main view files
    for (size_t i = 0; i < session.mainView.files.size(); ++i) {
        const auto& sessionFile = session.mainView.files[i];
        VFile vFile = sessionFileToVFile(sessionFile, 0); // Main view
        vFile.order = i;
        vFile.docOrder = static_cast<int>(i);
        vFile.isActive = session.mainView.activeIndex == vFile.docOrder;
        fileList.push_back(vFile);
    }
    
    // Process sub view files
    for (size_t i = 0; i < session.subView.files.size(); ++i) {
        const auto& sessionFile = session.subView.files[i];
        VFile vFile = sessionFileToVFile(sessionFile, 1); // Sub view
        vFile.order = i;
        vFile.docOrder = static_cast<int>(i);
        vFile.isActive = session.subView.activeIndex == vFile.docOrder;
        fileList.push_back(vFile);
    }
    
    // Remove files where session is not 0
    fileList.erase(
        std::remove_if(fileList.begin(), fileList.end(),
            [](const VFile& file) {
                return file.view != 0;
            }),
        fileList.end()
    );
    
    return fileList;
}

VFile sessionFileToVFile(const SessionFile& sessionFile, int view) {
    VFile vFile;
    vFile.order = 0; // Will be set by the caller
    
    // If backupFilePath is empty, filename contains the absolute path
    // Extract just the filename from the path
    if (sessionFile.backupFilePath.empty()) {
        std::filesystem::path filePath(sessionFile.filename);
        vFile.name = filePath.filename().string();
        vFile.path = sessionFile.filename;
        vFile.isEdited = false;
    } else {
        vFile.name = sessionFile.filename;
        vFile.path = sessionFile.backupFilePath;
        vFile.isEdited = true;
    }
    
    vFile.view = view; // Use the passed view parameter
    vFile.session = 0; // Default session index
    vFile.backupFilePath = sessionFile.backupFilePath;

    return vFile;
}






