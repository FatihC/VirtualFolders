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
#include "resource.h"
#include "Shlwapi.h"

#include "nlohmann/json.hpp"
using json = nlohmann::json;

#include <fstream>
#include <iostream>
#include <functional>
#include <algorithm>

#include <commctrl.h>
#include "model/VData.h"
#pragma comment(lib, "comctl32.lib")

#define NOMINMAX
#include <windowsx.h>
#include "ProcessCommands.h"
#include "RenameDialog.h"
#include "TreeViewManager.h"

#include <stdlib.h>

// External variables
extern CommonData commonData;








extern NPP::FuncItem menuDefinition[];  // Defined in Plugin.cpp
extern int menuItem_ToggleWatcher;      // Defined in Plugin.cpp


void writeJsonFile();
void resizeWatcherPanel();
void syncVDataWithOpenFiles(std::vector<VFile>& openFiles);




HWND watcherPanel = 0;



Scintilla::Position location = -1;
Scintilla::Position terminal = -1;




// Helper function to adjust global orders when moving a folder with all its contents
void adjustGlobalOrdersForFolderMove(int oldOrder, int newOrder, int folderItemCount) {
    // Get all files across the entire hierarchy
    vector<VFile*> allFiles = commonData.vData.getAllFiles();
    
    // Helper lambda to get all folders recursively
    std::function<void(vector<VFolder>&, vector<VFolder*>&)> getAllFolders = 
        [&](vector<VFolder>& folders, vector<VFolder*>& result) {
            for (auto& folder : folders) {
                result.push_back(&folder);
                getAllFolders(folder.folderList, result);
            }
        };
    
    vector<VFolder*> allFolders;
    getAllFolders(commonData.vData.folderList, allFolders);
    
    if (oldOrder < newOrder) {
        // Moving down - shift items in between to the left
        int posShiftAmount = newOrder - folderItemCount;
		int negShiftAmount = folderItemCount;
		int startOrder = oldOrder + folderItemCount; // Start from the next item

        
        // Adjust files
        for (auto* file : allFiles) {
            if (file->getOrder() >= startOrder && file->getOrder() < newOrder) {
				file->setOrder(file->getOrder() - negShiftAmount);
            }
        }
        
        // Adjust folders
        for (auto* folder : allFolders) {
            if (folder->getOrder() >= startOrder && folder->getOrder() < newOrder) {
                folder->setOrder(folder->getOrder() - negShiftAmount);
            }
        }
    } else {
        // Moving up - shift items in between to the right
        int shiftAmount = folderItemCount;
        
        // Adjust files
        for (auto* file : allFiles) {
            if (file->getOrder() >= newOrder && file->getOrder() < oldOrder) {
                file->setOrder(file->getOrder() + shiftAmount);
            }
        }
        
        // Adjust folders
        for (auto* folder : allFolders) {
            if (folder->getOrder() >= newOrder && folder->getOrder() < oldOrder) {
				folder->setOrder(folder->getOrder() + shiftAmount);
            }
        }
    }
}

// Helper function to adjust global orders for a single file move
void adjustGlobalOrdersForFileMove(int oldOrder, int newOrder) {
    // Get all files across the entire hierarchy
    vector<VFile*> allFiles = commonData.vData.getAllFiles();
    
    // Helper lambda to get all folders recursively
    std::function<void(vector<VFolder>&, vector<VFolder*>&)> getAllFolders = 
        [&](vector<VFolder>& folders, vector<VFolder*>& result) {
            for (auto& folder : folders) {
                result.push_back(&folder);
                getAllFolders(folder.folderList, result);
            }
        };
    
    vector<VFolder*> allFolders;
    getAllFolders(commonData.vData.folderList, allFolders);
    
    if (oldOrder < newOrder) {
        // Moving down - decrease orders of items in between
        for (auto* file : allFiles) {
            if (file->getOrder() > oldOrder && file->getOrder() < newOrder) {
				file->decrementOrder();
            }
        }
        for (auto* folder : allFolders) {
            if (folder->getOrder() > oldOrder && folder->getOrder() < newOrder) {
				folder->decrementOrder();
            }
        }
    } else {
        // Moving up - increase orders of items in between
        for (auto* file : allFiles) {
            if (file->getOrder() >= newOrder && file->getOrder() < oldOrder) {
				file->incrementOrder();
            }
        }
        for (auto* folder : allFolders) {
            if (folder->getOrder() >= newOrder && folder->getOrder() < oldOrder) {
                folder->incrementOrder();
            }
        }
    }
}

// Helper function to recursively adjust orders within a container
void adjustOrdersInContainer(vector<VFolder>& folders, vector<VFile>& files, int oldOrder, int newOrder) {
    if (oldOrder < newOrder) {
        // Moving down - decrease orders of items in between
        for (auto& folder : folders) {
            if (folder.getOrder() > oldOrder && folder.getOrder() < newOrder) {
                folder.decrementOrder();
            }
        }
        for (auto& file : files) {
            if (file.getOrder() > oldOrder && file.getOrder() < newOrder) {
				file.decrementOrder();
            }
        }
    } else {
        // Moving up - increase orders of items in between
        for (auto& folder : folders) {
            if (folder.getOrder() >= newOrder && folder.getOrder() < oldOrder) {
				folder.incrementOrder();
            }
        }
        for (auto& file : files) {
            if (file.getOrder() >= newOrder && file.getOrder() < oldOrder) {
				file.incrementOrder();
            }
        }
    }
}



FolderLocation findFolderLocation(VData& vData, int order) {
    FolderLocation location;
    
    // Check root level first
    for (auto& folder : vData.folderList) {
        if (folder.getOrder() == order) {
            location.folder = &folder;
            location.parentFolder = nullptr;  // Root level
            location.found = true;
            return location;
        }
    }
    
    // Recursively search in subfolders
    std::function<bool(VFolder&)> searchInFolder = [&](VFolder& parent) -> bool {
        for (auto& folder : parent.folderList) {
            if (folder.getOrder() == order) {
                location.folder = &folder;
                location.parentFolder = &parent;
                location.found = true;
                return true;
            }
            // Recursive search in this folder's subfolders
            if (searchInFolder(folder)) {
                return true;
            }
        }
        return false;
    };
    
    // Search in all root-level folders
    for (auto& rootFolder : vData.folderList) {
        if (searchInFolder(rootFolder)) {
            break;
        }
    }
    
    return location;
}


void refreshTree(HWND hTree, VData& vData) {
    // Clear existing tree
    TreeView_DeleteAllItems(hTree);
    
    // Rebuild tree with new order  
	vData.vDataSort();

    BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);
    
    int lastOrder = vData.folderList.empty() ? 0 : vData.folderList.back().getOrder();
    lastOrder = std::max(lastOrder, vData.fileList.empty() ? 0 : vData.fileList.back().getOrder());
    for (size_t pos = 0; pos <= lastOrder; pos) {
        optional<VFile*> vFile = vData.findFileByOrder(pos);
        if (!vFile) {
            optional<VFolder*> vFolder = vData.findFolderByOrder(pos);
            if (vFolder) {
                addFolderToTree(vFolder.value(), hTree, TVI_ROOT, pos, TVI_LAST);
            }
        } else {
            addFileToTree(vFile.value(), hTree, TVI_ROOT, isDarkMode, TVI_LAST);
            pos++;
        }
    }
}

void updateWatcherPanelUnconditional(UINT_PTR bufferID) {
    if(!commonData.isNppReady) {
        return;
    }

    if (ignoreSelectionChange) {
        ignoreSelectionChange = false;
		return; // Ignore this update
    }

    HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);
    BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);

    
    // Get current buffer ID. This is for if I lost the bufferID or could not get at the start
    if (bufferID <= 0) {
        bufferID = ::SendMessage(plugin.nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0); // does not take view as param
    }
    optional<VFile*> vFileOption = commonData.vData.findFileByBufferID(bufferID);
    if (!vFileOption) {
        int len = (int)::SendMessage(plugin.nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, bufferID, 0);
        wchar_t* filePath = new wchar_t[len + 1];
        ::SendMessage(plugin.nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, bufferID, (LPARAM)filePath);

        auto position = npp(NPPM_GETPOSFROMBUFFERID, bufferID, currentView);
        if (position == -1) {
            return;
        }

        if (position > 256 * 256) { // I am assuming there can not be more than 65536 buffers open
            int nbFiles = (int)npp(NPPM_GETNBOPENFILES, 0, currentView == MAIN_VIEW ? PRIMARY_VIEW : SECOND_VIEW);
            if (nbFiles == 1) {
                // Ignore this buffer-activated event
                return;
            }

        }

		// Check if file already exists in vData by path or name
        {
            string nppFileName = fromWchar(filePath);
            VFile* vFile = nullptr;
            if (nppFileName.find_first_of("\\\\") != string::npos) {
                vFile = commonData.vData.findFileByPath(nppFileName);
            }
            else {
                vFile = commonData.vData.findFileByName(nppFileName);
            }
            if (vFile) return;
        }




        // create vFile
		VFile newFile;
		newFile.bufferID = bufferID;
		newFile.setOrder(commonData.vData.getLastOrder() + 1);

        string filePathString = fromWchar(filePath);
        size_t lastSlash = filePathString.find_last_of("/\\");
        newFile.name = lastSlash != string::npos ? filePathString.substr(lastSlash + 1) : filePathString;

		newFile.path = fromWchar(filePath);
		newFile.view = currentView;
		newFile.session = 0;
		newFile.backupFilePath = "";
		newFile.isActive = true;
		commonData.vData.fileList.push_back(newFile);

        vFileOption = commonData.vData.findFileByBufferID(bufferID);
		VFile* vFilePtr = vFileOption.value();


        // create treeItem
		addFileToTree(vFilePtr, hTree, TVI_ROOT, isDarkMode, TVI_LAST);
        
        writeJsonFile();
    }
    else {
        // TODO: CHECK if it is a clone
        // kaç tane var
        vector<VFile*> allFilesWithBufferID = commonData.vData.getAllFilesByBufferID(bufferID);
        if (allFilesWithBufferID.size() == 1 && allFilesWithBufferID[0]->view != currentView) { // it is cloned
            VFolder* parentFolder = commonData.vData.findParentFolder(allFilesWithBufferID[0]->getOrder());
            
            VFile fileCopy = *allFilesWithBufferID[0];
            fileCopy.hTreeItem = nullptr;
            fileCopy.view = currentView;
            fileCopy.incrementOrder();

            adjustGlobalOrdersForFileMove(INT_MAX, fileCopy.getOrder());    // has to adjust orders to insert new item

            
            
            // create treeItem
            addFileToTree(&fileCopy, hTree, parentFolder ? parentFolder->hTreeItem : nullptr, isDarkMode, allFilesWithBufferID[0]->hTreeItem);

            if (parentFolder) parentFolder->fileList.push_back(fileCopy);
            else commonData.vData.fileList.push_back(fileCopy);
            vFileOption = commonData.vData.findFileByBufferID(bufferID, currentView);
            writeJsonFile();
        }
    }
    

    
    currentView = vFileOption.value()->view;

    if (TreeView_GetSelection(hTree) != vFileOption.value()->hTreeItem) {
        ignoreSelectionChange = true;
        HTREEITEM selectedItem = vFileOption.value()->hTreeItem;
        TreeView_SelectItem(hTree, selectedItem);
    }


}




void resizeWatcherPanel() {
    if (!watcherPanel || !IsWindow(watcherPanel)) return;
    
    // Get the current dialog position and size
    RECT rcDialog;
    GetWindowRect(watcherPanel, &rcDialog);
    
    // Get Notepad++ window dimensions
    RECT rcNpp;
    GetWindowRect(plugin.nppData._nppHandle, &rcNpp);
    
    // Calculate the maximum available width for the left panel
    // Use a larger portion of the window width for better usability
    int nppWidth = rcNpp.right - rcNpp.left;
    int nppHeight = rcNpp.bottom - rcNpp.top;
    int maxPanelWidth = nppWidth * 2 / 3;  // Allow up to 2/3 of the window width
    int panelWidth = maxPanelWidth;
    int panelHeight = nppHeight - 100; // Leave some space for menu/toolbar
    
    // Set minimum sizes to ensure usability
    const int MIN_WIDTH = 200;
    const int MIN_HEIGHT = 150;
    
    panelWidth = max(panelWidth, MIN_WIDTH);
    panelHeight = max(panelHeight, MIN_HEIGHT);
    
    // Resize the dialog to fill the available space
    SetWindowPos(watcherPanel, nullptr, 0, 0, panelWidth, panelHeight, 
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    
    // Resize the tree control to fill the dialog
    HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);
    if (hTree) {
        RECT rcClient;
        GetClientRect(watcherPanel, &rcClient);
        SetWindowPos(hTree, nullptr, 0, 0, 
                    rcClient.right - rcClient.left, 
                    rcClient.bottom - rcClient.top, 
                    SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        
        // Force the tree control to update its scrollbars
        InvalidateRect(hTree, nullptr, TRUE);
        UpdateWindow(hTree);
    }
}

void updateWatcherPanel(UINT_PTR bufferID, int activeView) {
    currentView = activeView;
    if (watcherPanel && IsWindowVisible(watcherPanel)) {}
    updateWatcherPanelUnconditional(bufferID);
}

void onFileClosed(UINT_PTR bufferID, int view) {
    HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);
    BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);

    optional<VFile*> vFileOpt = commonData.vData.findFileByBufferID(bufferID, view);
    if (!vFileOpt) {
        OutputDebugStringA("File not found in vData\n");
        return;
	}

    
    HTREEITEM hSelectedItem = vFileOpt.value()->hTreeItem;
    if (hSelectedItem) {
        // Remove the item from the tree
        TreeView_DeleteItem(hTree, hSelectedItem);
        
		VFile fileCopy = *(vFileOpt.value());
        VFolder* parentFolder = commonData.vData.findParentFolder(fileCopy.getOrder());
        if (parentFolder) {
            parentFolder->removeFile(fileCopy.getOrder());
        }
        else {
            commonData.vData.removeFile(fileCopy.getOrder());
        }

		adjustGlobalOrdersForFileMove(fileCopy.getOrder(), INT_MAX);

        
        // Write updated vData to JSON file
        writeJsonFile();
        
        
        OutputDebugStringA("File closed and removed from watcher panel\n");

		
	}
}

void onFileRenamed(UINT_PTR bufferID, wstring filepath, wstring fullpath) {
    HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);
    optional<VFile*> vFileOpt = commonData.vData.findFileByBufferID(bufferID);
    if (!vFileOpt) {
        OutputDebugStringA("File not found in vData\n");
        return;
    }
	string oldName = vFileOpt.value()->name;
        
	// Extract filename from filepath
	wstring fileName = filepath;
    // if file is saved extract filename
    if (vFileOpt.value()->backupFilePath.empty()) {
        size_t lastSlash = filepath.find_last_of(L"/\\");
        fileName = lastSlash != string::npos ? filepath.substr(lastSlash + 1) : filepath;
    }
	vFileOpt.value()->name = fromWchar(fileName.c_str());
	vFileOpt.value()->path = fromWchar(fullpath.c_str());


	HTREEITEM hSelectedItem = FindItemByLParam(hTree, TVI_ROOT, (LPARAM)(vFileOpt.value()->getOrder()));
    if (hSelectedItem) {
        // Update the item's text in the tree
        //std::wstring wideName(vFile.value()->name.begin(), vFile.value()->name.end());
        TVITEM tvi = { 0 };
        tvi.mask = TVIF_TEXT;
        tvi.hItem = hSelectedItem;
        tvi.pszText = const_cast<LPWSTR>(fileName.c_str());
        TreeView_SetItem(hTree, &tvi);
        
        // Write updated vData to JSON file
        writeJsonFile();
        
        OutputDebugStringA("File renamed and watcher panel updated\n");
	}
}

void toggleViewOfVFile(UINT_PTR bufferID)
{
    auto position1 = npp(NPPM_GETPOSFROMBUFFERID, bufferID, 0);
    int docView1 = (position1 >> 30) & 0x3;   // 0 = MAIN_VIEW, 1 = SUB_VIEW
    int docIndex1 = position1 & 0x3FFFFFFF;    // 0-based index

    auto position2 = npp(NPPM_GETPOSFROMBUFFERID, bufferID, 1);
    int docView2 = (position2 >> 30) & 0x3;   // 0 = MAIN_VIEW, 1 = SUB_VIEW
    int docIndex2 = position2 & 0x3FFFFFFF;    // 0-based index


    optional<VFile*> vFileMainOpt = commonData.vData.findFileByBufferID(bufferID, MAIN_VIEW);
    optional<VFile*> vFileSubOpt = commonData.vData.findFileByBufferID(bufferID, SUB_VIEW);

    if (docView1 == 0 && docView2 == 0) {
		// Main'de var sub'da yok
        if (!vFileMainOpt && vFileSubOpt) {
            LOG("TOGGLE sub to main");
            vFileSubOpt.value()->view = 0;
			currentView = 0;
		} else if (vFileMainOpt && vFileSubOpt) {
            LOG("REMOVE sub");
			onFileClosed(bufferID, 1);
            currentView = 0;
        } else {
            LOG("IMPOSSIBLE situation 1");
        }
    }
    else if (docView1 == 0 && docView2 == 1) {
		// Main'de var sub'da var
        LOG("IMPOSSIBLE situation 2");
    }
    else if (docView1 == 1 && docView2 == 1) {
        // Main'de yok sub'da var
        if (vFileMainOpt && !vFileSubOpt) {
            LOG("TOGGLE main to sub");
			vFileMainOpt.value()->view = 1;
            currentView = 1;
        }
        else if (vFileMainOpt && vFileSubOpt) {
            LOG("REMOVE main");
			onFileClosed(bufferID, 0);
            currentView = 1;
        }
        else {
            LOG("IMPOSSIBLE situation 3");
		}
    }
    

    // 1. Buffer moved
	//      position is from view=1. Main one closed already    view=0 var  view=1 yok
    //      -> toggle
    // 2. Clone moved
	//      position is from view=0. Sub one closed already     view=0 var  view=1 var
    //      -> remove view=1
    // 3. Clone closed
	//      position is from view=0. Sub one closed already     view=0 var  view=1 var
	//      -> remove view=1
    // 4. Cloned original moved
	//      position is from view=1. Main one closed already    view=0 var view=1 var
	//      -> remove view=0
    // 5. Cloned Original closed
    //      position is from view=1. Main one closed already
	//      -> remove view=0


    changeTreeItemIcon(bufferID, MAIN_VIEW);
    changeTreeItemIcon(bufferID, SUB_VIEW);

    writeJsonFile();
}

void syncVDataWithBufferIDs()
{
    int nbMainViewFile = (int)::SendMessage(plugin.nppData._nppHandle, NPPM_GETNBOPENFILES, 0, PRIMARY_VIEW);
    int nbSubViewFile = (int)::SendMessage(plugin.nppData._nppHandle, NPPM_GETNBOPENFILES, 0, SECOND_VIEW);
    int nbFile = nbMainViewFile + nbSubViewFile;

    wchar_t** fileNames = (wchar_t**)new wchar_t* [nbFile];
    vector<UINT_PTR> bufferIDVec(nbFile);

    int i = 0;
    for (; i < nbMainViewFile; )
    {
        LRESULT bufferID = ::SendMessage(plugin.nppData._nppHandle, NPPM_GETBUFFERIDFROMPOS, i, MAIN_VIEW);
        bufferIDVec[i] = bufferID;
        LRESULT len = ::SendMessage(plugin.nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, bufferID, (WPARAM)nullptr);
        fileNames[i] = new wchar_t[len + 1];
        ::SendMessage(plugin.nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, bufferID, (WPARAM)fileNames[i]);
        ++i;
    }


    for (int j = 0; j < nbSubViewFile; ++j)
    {
        LRESULT bufferID = ::SendMessage(plugin.nppData._nppHandle, NPPM_GETBUFFERIDFROMPOS, j, SUB_VIEW);
        bufferIDVec[i] = bufferID;
        LRESULT len = ::SendMessage(plugin.nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, bufferID, (WPARAM)nullptr);
        fileNames[i] = new wchar_t[len + 1];
        ::SendMessage(plugin.nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, bufferID, (WPARAM)fileNames[i]);
        ++i;
    }

    int view = 0;
    for (int k = 0; k < nbFile; k++) {
		if (k == nbMainViewFile) view = 1;

        string nppFileName = fromWchar(fileNames[k]);
        VFile* vFile = nullptr;
        if (nppFileName.find_first_of("\\\\") != string::npos) {
            vFile = commonData.vData.findFileByPath(nppFileName, view);
        }
        else {
            vFile = commonData.vData.findFileByName(nppFileName, view);
        }
        if (!vFile) continue;
        if (vFile->bufferID > 0) continue;
        vFile->bufferID = bufferIDVec[k];
    }




    for (int k = 0; k < nbFile; k++)
    {
        delete[] fileNames[k];
    }
    delete[] fileNames;
}

void toggleWatcherPanelWithList() {
    if (!watcherPanel) {
        watcherPanel = CreateDialog(plugin.dllInstance, MAKEINTRESOURCE(IDD_FILEVIEW), plugin.nppData._nppHandle, fileViewDialogProc);
        
        NPP::tTbData dock;
        HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);

        // Resize dialog to match left panel size
        resizeWatcherPanel();

		commonData.hTree = hTree;

        setFontSize();



        
        TCHAR configDir[MAX_PATH];
        ::SendMessage(plugin.nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)configDir);
        jsonFilePath = std::wstring(configDir) + L"\\virtualfolders.json";

        // read JSON
        json vDataJson = loadVDataFromFile(jsonFilePath);
        commonData.vData = vDataJson.get<VData>();

        commonData.openFiles = listOpenFiles();
        
        // Sync vData with open files
        syncVDataWithOpenFiles(commonData.openFiles);

		
        writeJsonFile();
        

        commonData.vData.vDataSort();

        syncVDataWithBufferIDs();


		
        int lastOrder = commonData.vData.folderList.empty() ? 0 : commonData.vData.folderList.back().getOrder();
		lastOrder = std::max(lastOrder, commonData.vData.fileList.empty() ? 0 : commonData.vData.fileList.back().getOrder());

        BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);

        for (size_t pos = 0; pos <= lastOrder; pos) {
			auto vFile = commonData.vData.findFileByOrder(pos);
            if (!vFile) {
                auto vFolder = commonData.vData.findFolderByOrder(pos);
                if (vFolder) {
                    addFolderToTree(vFolder.value(), hTree, TVI_ROOT, pos, TVI_LAST);
                }
            }
            else {
                addFileToTree(vFile.value(), hTree, TVI_ROOT, isDarkMode, TVI_LAST);
                pos++;
            }

        }
        

        dock.hClient = watcherPanel;
        dock.pszName = L"Virtual Folders";  // title bar text (caption in dialog is replaced)
        dock.dlgID = menuItem_ToggleWatcher;          // zero-based position in menu to recall dialog at next startup
        dock.uMask = DWS_DF_CONT_LEFT | DWS_ICONTAB;
        dock.pszModuleName = L"VFolders.dll";        // plugin module name
        HICON hIcon = LoadIcon(plugin.dllInstance, MAKEINTRESOURCE(IDI_FOLDER_YELLOW));
        dock.hIconTab = hIcon;


        npp(NPPM_DMMREGASDCKDLG, 0, &dock);

        OutputDebugStringA("Watch Panel Create\n");
    }
    else if (IsWindowVisible(watcherPanel)) {
        npp(NPPM_DMMHIDE, 0, watcherPanel);
        OutputDebugStringA("Watch Panel Hide\n");
        // Update tab selection state
        commonData.virtualFoldersTabSelected = false;
    }
    else {
        updateWatcherPanelUnconditional(-1);
        npp(NPPM_DMMSHOW, 0, watcherPanel);
        OutputDebugStringA("Watch Panel Show\n");
        // Update tab selection state
        commonData.virtualFoldersTabSelected = true;
    }
    HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);
	commonData.hTree = hTree;
}

void syncVDataWithOpenFiles(vector<VFile>& openFiles) {

    for (int i = 0; i < openFiles.size(); i++) {
        VFile* jsonVFile = commonData.vData.findFileByPath(openFiles[i].path, openFiles[i].view);
        if (!jsonVFile) {
            commonData.vData.fileList.push_back(openFiles[i]);
            continue;
        }

        if (jsonVFile->path == openFiles[i].path) {
            if (jsonVFile->backupFilePath != openFiles[i].backupFilePath) {
                jsonVFile->name = openFiles[i].name;
                jsonVFile->backupFilePath = openFiles[i].backupFilePath;
            }
            else {
                jsonVFile->isActive = openFiles[i].isActive;
            }
            jsonVFile->isEdited = openFiles[i].isEdited;
            jsonVFile->view = openFiles[i].view;
            jsonVFile->session = openFiles[i].session;
            jsonVFile->isReadOnly = openFiles[i].isReadOnly;
        }
    }

    // Collect all vFiles that are now in vData but not in openFiles.
    vector<VFile*> allFiles = commonData.vData.getAllFiles();
    for (int i = 0; i < allFiles.size(); i++) {
        for (int j = 0; j < openFiles.size(); j++) {
            if (allFiles[i]->path == openFiles[j].path && allFiles[i]->view == openFiles[j].view) {
                break;
            }
            if (j == openFiles.size() - 1) {
                // Not found in openFiles, remove it from vData
                //commonData.vData.removeFile(allFiles[i]->getOrder());

                VFile fileCopy = *(allFiles[i]);
                VFolder* parentFolder = commonData.vData.findParentFolder(fileCopy.getOrder());
                if (parentFolder) {
                    parentFolder->removeFile(fileCopy.getOrder());
                }
                else {
                    commonData.vData.removeFile(fileCopy.getOrder());
                }

                adjustGlobalOrdersForFileMove(fileCopy.getOrder(), INT_MAX);


            }
        }
    }

    // TODO: fix orders


    /*vData.fileList.erase(
        std::remove_if(vData.fileList.begin(), vData.fileList.end(),
            [&openFiles](const VFile& file) {
                auto it = std::find_if(openFiles.begin(), openFiles.end(),
                    [&file](const VFile& openFile) {
                        return openFile.path == file.path && openFile.view == file.view;
                    });
                return it == openFiles.end();
            }),
        vData.fileList.end()
    );*/



}

wchar_t* toWchar(const std::string& str) {
	static wchar_t buffer[256];
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer, 256);
	return buffer;
}

string fromWchar(const wchar_t* wstr) {
    if (!wstr) return {};

    // length of wide string excluding the null terminator
    int len = static_cast<int>(wcslen(wstr));

    if (len == 0) return {};

    // get required size for conversion
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr, len, nullptr, 0, nullptr, nullptr);

    std::string result(sizeNeeded, '\0');

    WideCharToMultiByte(CP_UTF8, 0, wstr, len, &result[0], sizeNeeded, nullptr, nullptr);

    return result;
}

wstring toWstring(const string& str) {
    if (str.empty()) return L"";

    // Get required size (in wchar_t�s)
    int size_needed = MultiByteToWideChar(
        CP_UTF8,            // source is UTF-8
        0,                  // no special flags
        str.c_str(),        // input string
        (int)str.size(),    // number of chars to convert
        nullptr,            // don�t output yet
        0                   // request size only
    );

    // Allocate std::wstring with proper size
    wstring wstr(size_needed, 0);

    // Do the actual conversion
    MultiByteToWideChar(
        CP_UTF8, 0,
        str.c_str(), (int)str.size(),
        &wstr[0], size_needed
    );

    return wstr;
}
