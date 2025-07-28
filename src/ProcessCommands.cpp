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

void setVFileInfo(VFile& vFile) {
    // Convert string path to wstring for Windows API
    std::wstring wPath(vFile.path.begin(), vFile.path.end());
    
    // Open file handle
    HANDLE hFile = CreateFileW(wPath.c_str(), 
                              GENERIC_READ, 
                              FILE_SHARE_READ, 
                              NULL, 
                              OPEN_EXISTING, 
                              FILE_ATTRIBUTE_NORMAL, 
                              NULL);
    
    if (hFile != INVALID_HANDLE_VALUE) {
        // Get creation time
        FILETIME creationTime, lastAccessTime, lastWriteTime;
        if (GetFileTime(hFile, &creationTime, &lastAccessTime, &lastWriteTime)) {
            vFile.creationTime = creationTime;
        }
        
        // Try File ID first (most reliable)
        FILE_ID_INFO fileIdInfo;
        DWORD bytesReturned;
        
        if (GetFileInformationByHandleEx(hFile, FileIdInfo, &fileIdInfo, 
                                        sizeof(fileIdInfo))) {
            // Convert FILE_ID_128 to string (it's a 16-byte array)
            std::wstring fileIdStr = L"FILEID_";
            for (int i = 0; i < 16; i++) {
                wchar_t hex[3];
                swprintf_s(hex, 3, L"%02X", fileIdInfo.FileId.Identifier[i]);
                fileIdStr += hex;
            }
            vFile.id = fileIdStr;
        } else {
            // Fallback to File Reference Number
            BY_HANDLE_FILE_INFORMATION fileInfo;
            if (GetFileInformationByHandle(hFile, &fileInfo)) {
                ULONGLONG refNum = (static_cast<ULONGLONG>(fileInfo.nFileIndexHigh) << 32) | 
                                   fileInfo.nFileIndexLow;
                vFile.id = L"REFNUM_" + std::to_wstring(refNum);
            } else {
                // Final fallback: path hash
                vFile.id = L"HASH_" + std::to_wstring(std::hash<std::wstring>{}(wPath));
            }
        }
        
        CloseHandle(hFile);
    } else {
        // File couldn't be opened, set default values
        vFile.id = L"";
        vFile.creationTime = {0, 0};
    }
}


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
	listOpenFiles(); // Call the function to list open files
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
        VFile vFile = sessionFileToVFile(sessionFile);
        fileList.push_back(vFile);
    }
    
    // Process sub view files
    for (size_t i = 0; i < session.subView.files.size(); ++i) {
        const auto& sessionFile = session.subView.files[i];
        VFile vFile = sessionFileToVFile(sessionFile);
        fileList.push_back(vFile);
    }
    
    return fileList;
}

VFile sessionFileToVFile(const SessionFile sessionFile) {
    VFile vFile;
    vFile.name = sessionFile.filename;
    vFile.path = sessionFile.backupFilePath.empty() ? sessionFile.filename : sessionFile.backupFilePath;
    vFile.view = 0; // Main view
    vFile.session = 0; // Default session index
    vFile.backupFilePath = sessionFile.backupFilePath;

    return vFile;
}






