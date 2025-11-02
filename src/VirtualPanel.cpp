// This file is part of VirtualFolders.
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


#include "TreeViewManager.h"

// External variables
extern CommonData commonData;








extern NPP::FuncItem menuDefinition[];  // Defined in Plugin.cpp
extern int menuItem_ToggleVirtualPanel;      // Defined in Plugin.cpp

extern void showCorruptionDialog(VFolder hOldFolder, VFolder hNewFolder, int hOldOrder, int hNewOrder); // Defined in CorruptionDialog.cpp


void writeJsonFile();
void resizeVirtualPanel();
void syncVDataWithOpenFiles(std::vector<VFile>& openFiles);
void fixRootVFolderJSON();
bool checkRootVFolderJSON();


HWND virtualPanelWnd = 0;


Scintilla::Position location = -1;
Scintilla::Position terminal = -1;




// Helper function to adjust global orders when moving a folder with all its contents
void adjustGlobalOrdersForFolderMove(int oldOrder, int newOrder, int folderItemCount) {
    // Get all files across the entire hierarchy
    vector<VFile*> allFiles = commonData.rootVFolder.getAllFiles();
    
    // Helper lambda to get all folders recursively
    std::function<void(vector<VFolder>&, vector<VFolder*>&)> getAllFolders = 
        [&](vector<VFolder>& folders, vector<VFolder*>& result) {
            for (auto& folder : folders) {
                result.push_back(&folder);
                getAllFolders(folder.folderList, result);
            }
        };
    
    vector<VFolder*> allFolders;
    getAllFolders(commonData.rootVFolder.folderList, allFolders);
    
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
    vector<VFile*> allFiles = commonData.rootVFolder.getAllFiles();
    
    // Helper lambda to get all folders recursively
    std::function<void(vector<VFolder>&, vector<VFolder*>&)> getAllFolders = 
        [&](vector<VFolder>& folders, vector<VFolder*>& result) {
            for (auto& folder : folders) {
                result.push_back(&folder);
                getAllFolders(folder.folderList, result);
            }
        };
    
    vector<VFolder*> allFolders;
    getAllFolders(commonData.rootVFolder.folderList, allFolders);
    
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



FolderLocation findFolderLocation(int order) {
    FolderLocation location;
    
    // Check root level first
    for (auto& folder : commonData.rootVFolder.folderList) {
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
    for (auto& rootFolder : commonData.rootVFolder.folderList) {
        if (searchInFolder(rootFolder)) {
            break;
        }
    }
    
    return location;
}

void updateVirtualPanel(UINT_PTR bufferID, int activeView) {
    currentView = activeView;

    if(!commonData.isNppReady && commonData.rootVFolder.getLastOrder() >= 0) {  // if there is no buffer a default one comes. We should show that in the tree
        return;
    }

    if (ignoreSelectionChange) {
        ignoreSelectionChange = false;
		return; // Ignore this update
    }

    HWND hTree = GetDlgItem(virtualPanelWnd, IDC_TREE1);
    BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);

    
    // Get current buffer ID. This is for if I lost the bufferID or could not get at the start
    if (bufferID <= 0) {
        bufferID = ::SendMessage(plugin.nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0); // does not take view as param
    }
    optional<VFile*> vFileOption = commonData.rootVFolder.findFileByBufferID(bufferID);
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
                vFile = commonData.rootVFolder.findFileByPath(nppFileName);
            }
            else {
                vFile = commonData.rootVFolder.findFileByName(nppFileName);
            }
            if (vFile) return;
        }




        // create vFile
		VFile newFile;
		newFile.bufferID = bufferID;
		newFile.setOrder(commonData.rootVFolder.getLastOrder() + 1);

        string filePathString = fromWchar(filePath);
        size_t lastSlash = filePathString.find_last_of("/\\");
        newFile.name = lastSlash != string::npos ? filePathString.substr(lastSlash + 1) : filePathString;

		newFile.path = fromWchar(filePath);
		newFile.view = currentView;
		newFile.session = 0;
		newFile.backupFilePath = "";
		newFile.isActive = true;
		commonData.rootVFolder.fileList.push_back(newFile);

        vFileOption = commonData.rootVFolder.findFileByBufferID(bufferID);
		VFile* vFilePtr = vFileOption.value();


        // create treeItem
		addFileToTree(vFilePtr, hTree, TVI_ROOT, isDarkMode, TVI_LAST);
        
        writeJsonFile();
    }
    else {
        // CHECK if it is a clone
        vector<VFile*> allFilesWithBufferID = commonData.rootVFolder.getAllFilesByBufferID(bufferID);
        if (allFilesWithBufferID.size() == 1 && allFilesWithBufferID[0]->view != currentView) { // it is cloned
            VFolder* parentFolder = commonData.rootVFolder.findParentFolder(allFilesWithBufferID[0]->getOrder());
            
            VFile fileCopy = *allFilesWithBufferID[0];
            fileCopy.hTreeItem = nullptr;
            fileCopy.view = currentView;
            fileCopy.incrementOrder();

            adjustGlobalOrdersForFileMove(INT_MAX, fileCopy.getOrder());    // has to adjust orders to insert new item

            
            
            // create treeItem
            addFileToTree(&fileCopy, hTree, parentFolder ? parentFolder->hTreeItem : nullptr, isDarkMode, allFilesWithBufferID[0]->hTreeItem);

            if (parentFolder) parentFolder->fileList.push_back(fileCopy);
            else commonData.rootVFolder.fileList.push_back(fileCopy);
            vFileOption = commonData.rootVFolder.findFileByBufferID(bufferID, currentView);
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




void resizeVirtualPanel() {
    if (!virtualPanelWnd || !IsWindow(virtualPanelWnd)) return;
    
    // Get the current dialog position and size
    RECT rcDialog;
    GetWindowRect(virtualPanelWnd, &rcDialog);
    
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
    SetWindowPos(virtualPanelWnd, nullptr, 0, 0, panelWidth, panelHeight,
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    
    // Resize the tree control to fill the dialog
    HWND hTree = GetDlgItem(virtualPanelWnd, IDC_TREE1);
    if (hTree) {
        RECT rcClient;
        GetClientRect(virtualPanelWnd, &rcClient);
        SetWindowPos(hTree, nullptr, 0, 0, 
                    rcClient.right - rcClient.left, 
                    rcClient.bottom - rcClient.top, 
                    SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        
        // Force the tree control to update its scrollbars
        InvalidateRect(hTree, nullptr, TRUE);
        UpdateWindow(hTree);
    }
}

void onFileClosed(UINT_PTR bufferID, int view) {
    HWND hTree = GetDlgItem(virtualPanelWnd, IDC_TREE1);
    BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);

    optional<VFile*> vFileOpt = commonData.rootVFolder.findFileByBufferID(bufferID, view);
    if (!vFileOpt) {
        OutputDebugStringA("File not found in vData\n");
        return;
	}

    
    HTREEITEM hSelectedItem = vFileOpt.value()->hTreeItem;
    if (hSelectedItem) {
        // Remove the item from the tree
        TreeView_DeleteItem(hTree, hSelectedItem);
        
		VFile fileCopy = *(vFileOpt.value());
        VFolder* parentFolder = commonData.rootVFolder.findParentFolder(fileCopy.getOrder());
        if (parentFolder) {
            parentFolder->removeFile(fileCopy.getOrder());
        }
        else {
            commonData.rootVFolder.removeFile(fileCopy.getOrder());
        }

		adjustGlobalOrdersForFileMove(fileCopy.getOrder(), INT_MAX);

        
        // Write updated vData to JSON file
        writeJsonFile();
        
        
        LOG("File closed and removed from tree panel");

		
	}
}

void onFileRenamed(UINT_PTR bufferID, wstring filepath, wstring fullpath) {
    HWND hTree = GetDlgItem(virtualPanelWnd, IDC_TREE1);
    optional<VFile*> vFileOpt = commonData.rootVFolder.findFileByBufferID(bufferID);
    if (!vFileOpt) {
        LOG("File not found in vData");
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
        
        OutputDebugStringA("File renamed and tree panel updated\n");
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


    optional<VFile*> vFileMainOpt = commonData.rootVFolder.findFileByBufferID(bufferID, MAIN_VIEW);
    optional<VFile*> vFileSubOpt = commonData.rootVFolder.findFileByBufferID(bufferID, SUB_VIEW);

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
            vFile = commonData.rootVFolder.findFileByPath(nppFileName, view);
        }
        else {
            vFile = commonData.rootVFolder.findFileByName(nppFileName, view);
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

void toggleVirtualPanelWithList() {
    if (!virtualPanelWnd) {
        virtualPanelWnd = CreateDialog(plugin.dllInstance, MAKEINTRESOURCE(IDD_FILEVIEW), plugin.nppData._nppHandle, fileViewDialogProc);
        
        
        HWND hTree = GetDlgItem(virtualPanelWnd, IDC_TREE1);

        try {
            // Resize dialog to match left panel size
            resizeVirtualPanel();

		    commonData.hTree = hTree;

            setFontSize();



        
            TCHAR configDir[MAX_PATH];
            SendMessage(plugin.nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)configDir);
            filesystem::path configFolder(configDir);
            filesystem::path virtualFolderPluginFolder = configFolder.parent_path() / L"VirtualFolders";
            filesystem::create_directories(virtualFolderPluginFolder);


            jsonFilePath = virtualFolderPluginFolder.wstring() + L"\\VFolders-storage.json";


            // read JSON
            json rootVFolderJson = loadVDataFromFile(jsonFilePath);
            commonData.rootVFolder = rootVFolderJson.get<VFolder>();

            commonData.openFiles = listOpenFiles();
        
            // Sync rootVFolder with open files
            syncVDataWithOpenFiles(commonData.openFiles);

            commonData.rootVFolder.vFolderSort();
            commonData.rootVFolder.setOrder(-1);

            writeJsonFile();

            syncVDataWithBufferIDs();

            vector<VBase*> allChildren = commonData.rootVFolder.getAllChildren();


			
            BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);
            if (checkRootVFolderJSON()) {
                showCorruptionDialog(commonData.rootVFolder, commonData.rootVFolder, 0, 0);
                fixRootVFolderJSON(); // uncomment on production
            }

            int lastOrder = commonData.rootVFolder.getLastOrder();
            for (ssize_t pos = 0; pos <= lastOrder; pos) {
				optional<VBase*> childOpt = commonData.rootVFolder.getDirectChildByOrder(pos);
                if (!childOpt) {
                    // This should not happen, but just in case. 
                    pos++;
                }
                else if (auto file = dynamic_cast<VFile*>(childOpt.value())) {
                    addFileToTree(file, hTree, TVI_ROOT, isDarkMode, TVI_LAST);
                    pos++;
                }
                else if (auto folder = dynamic_cast<VFolder*>(childOpt.value())) {
                    addFolderToTree(folder, hTree, TVI_ROOT, pos, TVI_LAST);
                }
            }


            static std::wstring pluginTitleStr;
            pluginTitleStr = commonData.translator->getTextW("IDM_PLUGIN_TITLE");
            static const wchar_t* pluginTitle = pluginTitleStr.c_str();
        

            dock.hClient = virtualPanelWnd;
            dock.pszName = pluginTitle;  // title bar text (caption in dialog is replaced)
            dock.dlgID = menuItem_ToggleVirtualPanel;          // zero-based position in menu to recall dialog at next startup
            dock.uMask = DWS_DF_CONT_LEFT | DWS_ICONTAB;
            dock.pszModuleName = L"VirtualFolders.dll";        // plugin module name
            HICON hIcon = LoadIcon(plugin.dllInstance, MAKEINTRESOURCE(IDI_FOLDER_YELLOW));
            dock.hIconTab = hIcon;


            npp(NPPM_DMMREGASDCKDLG, 0, &dock);
        }
        catch (int x) 
        {
            LOG("We caught an int exception with value: [{}]", x);
        }


        

        LOG("Tree Panel Created");
    }
    else if (IsWindowVisible(virtualPanelWnd)) {
        npp(NPPM_DMMHIDE, 0, virtualPanelWnd);
        LOG("Tree Panel Hide");
        // Update tab selection state
        commonData.virtualFoldersTabSelected = false;
    }
    else {
        updateVirtualPanel(-1, currentView);
        npp(NPPM_DMMSHOW, 0, virtualPanelWnd);
        LOG("Tree Panel Show");
        // Update tab selection state
        commonData.virtualFoldersTabSelected = true;
    }
    HWND hTree = GetDlgItem(virtualPanelWnd, IDC_TREE1);
	commonData.hTree = hTree;
}

void syncVDataWithOpenFiles(vector<VFile>& openFiles) {
    vector<VFile*> allJsonVFiles = commonData.rootVFolder.getAllFiles();
    for (VFile* vFile : allJsonVFiles)
    {
        if (vFile->path != vFile->name) {
            continue;
        }
        // newly created buffers backup files are unreachable during session
        // so I here their name and path are equal (eg: new 1)
		// Check if this file exists in openFiles
        bool found = false;
        for (const VFile& openFile : openFiles) {
            if (openFile.name == vFile->name && openFile.view == vFile->view) {
                found = true;
				vFile->backupFilePath = openFile.backupFilePath;
				vFile->path = openFile.path;
                break;
            }
        }
        if (!found) {
            // File no longer open, remove it from rootVFolder
            VFile fileCopy = *vFile;
            VFolder* parentFolder = commonData.rootVFolder.findParentFolder(fileCopy.getOrder());
            if (parentFolder) {
                parentFolder->removeFile(fileCopy.getOrder());
            }
            else {
                commonData.rootVFolder.removeFile(fileCopy.getOrder());
			}
        }

    }


    for (int i = 0; i < openFiles.size(); i++) {
        VFile* jsonVFile = commonData.rootVFolder.findFileByPath(openFiles[i].path, openFiles[i].view);
        if (!jsonVFile) {
            commonData.rootVFolder.fileList.push_back(openFiles[i]);
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

    // Collect all vFiles that are now in rootVFolder but not in openFiles.
    vector<VFile*> allFiles = commonData.rootVFolder.getAllFiles();
    for (int i = 0; i < allFiles.size(); i++) {
		bool found = false;
        for (int j = 0; j < openFiles.size(); j++) {
            if (allFiles[i]->path == openFiles[j].path && allFiles[i]->view == openFiles[j].view) {
				found = true;
                break;
            }
        }
        if (!found) {
            // Not found in openFiles, remove it from rootVFolder

            VFile fileCopy = *(allFiles[i]);
            VFolder* parentFolder = commonData.rootVFolder.findParentFolder(fileCopy.getOrder());
            if (parentFolder) {
                parentFolder->removeFile(fileCopy.getOrder());
            }
            else {
                commonData.rootVFolder.removeFile(fileCopy.getOrder());
            }

            adjustGlobalOrdersForFileMove(fileCopy.getOrder(), INT_MAX);
        }
    }

}

bool checkRootVFolderJSON() {
    // Corruptions in json
    // 1. Two items with same order
    //  Fix: check if any gap above or below. If not, assign new order to one of them.
    int lastOrder = commonData.rootVFolder.getLastOrder();
    vector<VBase*> allChildren = commonData.rootVFolder.getAllChildren();
    for (int i = 0; i < allChildren.size(); i++) {
        for (int j = i + 1; j < allChildren.size(); j++) {
            if (allChildren[i]->getOrder() == allChildren[j]->getOrder()) {
                // Found duplicate orders
                LOG("checkRootVFolderJSON: Duplicate orders");
                return true;
            }
        }
    }




    lastOrder = commonData.rootVFolder.getLastOrder();
    // 4. Negative orders except root folder (-1)
    //  Fix: If positive order exists, assign new order with lastOrder+1
    allChildren = commonData.rootVFolder.getAllChildren();
    for (auto* child : allChildren) {
        if (child->getOrder() >= 0) continue;
        if (child->getOrder() == -1 && dynamic_cast<VFolder*>(child) == &commonData.rootVFolder) continue;
        
        LOG("checkRootVFolderJSON: Negative Order");
        return true;
    }

    // 2. Gaps in orders
    for (int i = 0; i < lastOrder; i++) {
        optional<VBase*> child = commonData.rootVFolder.getChildByOrder(i);
        if (child) continue;

        LOG("checkRootVFolderJSON: Gaps in orders");
        return true;
    }


    // 3. Order not sequential in a folder

    commonData.rootVFolder.vFolderSort();
    std::function<bool(VFolder*, ssize_t&)> isTheTreeCorrupt =
        [&](VFolder* folder, ssize_t& startPos) -> bool {

        int lastOrder = commonData.rootVFolder.getLastOrder();

        while (startPos <= lastOrder) {
            optional<VBase*> childOpt = folder->getDirectChildByOrder(startPos);
            if (!childOpt) {
                return true;
            }
            startPos++;
            if (auto subFolder = dynamic_cast<VFolder*>(childOpt.value())) {
                bool isCorrupt = isTheTreeCorrupt(subFolder, startPos);
                if (isCorrupt) {
                    return true;
                }
            }
        }
        return false;
        };

    ssize_t startPos = 0;
    bool treeIsCorrupt = isTheTreeCorrupt(&commonData.rootVFolder, startPos);

    LOG("Finished checking rootVFolder JSON: [{}]", treeIsCorrupt);
    
    
    return treeIsCorrupt;
}

void fixRootVFolderJSON() {
    // Corruptions in json
    // 1. Two items with same order
    //  Fix: check if any gap above or below. If not, assign new order to one of them.
    int lastOrder = commonData.rootVFolder.getLastOrder();
    vector<VBase*> allChildren = commonData.rootVFolder.getAllChildren();
    for (int i = 0; i < allChildren.size(); i++) {
        for (int j = i + 1; j < allChildren.size(); j++) {
            if (allChildren[i]->getOrder() == allChildren[j]->getOrder()) {
                // Found duplicate orders
                int dupOrder = allChildren[i]->getOrder();
                optional<VBase*> lowerNeighbor = commonData.rootVFolder.getChildByOrder(dupOrder - 1);
                optional<VBase*> upperNeighbor = commonData.rootVFolder.getChildByOrder(dupOrder + 1);
                if (!lowerNeighbor && dupOrder > 0) {
                    // No lower neighbor, shift down
                    allChildren[j]->setOrder(dupOrder - 1);
                    //adjustGlobalOrdersForFileMove(dupOrder - 1, dupOrder);
                }
                else if (!upperNeighbor) {
                    // No upper neighbor, shift up
                    allChildren[j]->setOrder(dupOrder + 1);
                    //adjustGlobalOrdersForFileMove(dupOrder + 1, dupOrder);
                }
                else {
                    // Both neighbors exist, assign new order at the end
                    allChildren[j]->setOrder(lastOrder + 1);
                    lastOrder++;
                }
            }
        }
	}




    lastOrder = commonData.rootVFolder.getLastOrder();
    // 4. Negative orders except root folder (-1)
    //  Fix: If positive order exists, assign new order with lastOrder+1
    allChildren = commonData.rootVFolder.getAllChildren();
    for (auto* child : allChildren) {
        if (child->getOrder() >= 0) continue;
        if (child->getOrder() == -1 && dynamic_cast<VFolder*>(child) == &commonData.rootVFolder) continue;
        optional<VBase*> positiveChild = commonData.rootVFolder.getChildByOrder(child->getOrder() * -1);
        if (!positiveChild) {
			child->setOrder(child->getOrder() * -1);
        } else {
            child->setOrder(lastOrder + 1);
			lastOrder++;
		}
	}

    // 2. Gaps in orders
    for (int i = 0; i < lastOrder; i++) {
        optional<VBase*> child = commonData.rootVFolder.getChildByOrder(i);
        if (child) continue;

        adjustGlobalOrdersForFileMove(i, lastOrder + 1);
        lastOrder--;
        i--;
    }


    // 3. Order not sequential in a folder

	commonData.rootVFolder.vFolderSort();
    std::function<bool(VFolder*, ssize_t)> isTheTreeCorrupt =
        [&](VFolder* folder, ssize_t startPos) -> bool {
        int lastOrder = folder->fileList.empty() ? 0 : folder->fileList.back().getOrder();
        lastOrder = std::max(lastOrder, folder->folderList.empty() ? 0 : folder->folderList.back().getOrder());

        while (startPos <= lastOrder) {
            optional<VBase*> childOpt = commonData.rootVFolder.getDirectChildByOrder(startPos);
            if (!childOpt) {
                return true;
            }
            startPos++;
            if (auto subFolder = dynamic_cast<VFolder*>(childOpt.value())) {
                bool isCorrupt = isTheTreeCorrupt(subFolder, startPos);
                if (isCorrupt) return true;
            }
        }
        return false;
        };

    ssize_t startPos = 0;
    bool treeIsCorrupt = isTheTreeCorrupt(&commonData.rootVFolder, startPos);


    // Nuclear option
    if (treeIsCorrupt) {
        startPos = 0;
        commonData.rootVFolder.resetOrders(startPos);
    }


    //json vDataJson = commonData.rootVFolder;
    //LOG("fixed json: [{}]", vDataJson.dump(4));


    LOG("Finished fixing rootVFolder JSON");
}