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

#include "CommonData.h"
#include "model/VData.h"
#include "ProcessCommands.h"
#include "resource.h"
#include "Translator.h"




extern void updateStatusDialog();
extern void updateVirtualPanel(UINT_PTR bufferID, int activeView);
extern void onFileClosed(UINT_PTR bufferID, int view = 0);
extern void onFileRenamed(UINT_PTR bufferID, wstring filepath, wstring fullpath);
void scnSavePointEvent(UINT_PTR bufferID, bool isSavePoint);
extern void changeTreeItemIcon(UINT_PTR bufferID, int view);
extern void syncVDataWithBufferIDs();
extern void toggleViewOfVFile(UINT_PTR bufferID);



// External variables
extern HWND virtualPanelWnd;
extern string fromWchar(const wchar_t* wstr);


wchar_t* getFullPathFromBufferID(UINT_PTR bufferID);
wchar_t* getPluginHomePath();
int GetActiveViewForBuffer(UINT_PTR bufferID);
void loadLocalization();


void scnModified(const Scintilla::NotificationData* scnp) {
    using Scintilla::FlagSet;
    if (FlagSet(scnp->modificationType, Scintilla::ModificationFlags::InsertText)) ++commonData.insertsCounted;
    else if (FlagSet(scnp->modificationType, Scintilla::ModificationFlags::DeleteText)) ++commonData.deletesCounted;
    else return;
    //updateVirtualPanel();
}

void scnSavePointEvent(UINT_PTR bufferID, bool isSavePoint) {
    if (!commonData.isNppReady) return;
    if (bufferID == 0) {
        bufferID = ::SendMessage(plugin.nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0);
    }
    if (commonData.bufferStates.find(bufferID) == commonData.bufferStates.end()) {
        commonData.bufferStates[bufferID] = isSavePoint;
    }
    
    if (isSavePoint && !commonData.bufferStates[bufferID]) {
        // Document has been saved update icon
        commonData.bufferStates[bufferID] = true;
    } else if(!isSavePoint && commonData.bufferStates[bufferID]){
        // Document has been modified since last save
        commonData.bufferStates[bufferID] = false;
    }

    changeTreeItemIcon(bufferID, MAIN_VIEW);
    changeTreeItemIcon(bufferID, SUB_VIEW);
}


void scnUpdateUI(const Scintilla::NotificationData* /* scnp */) {
}


void scnZoom(const Scintilla::NotificationData* /* scnp */) {
}


void bufferActivated(const NMHDR* nmhdr) {
    UINT_PTR bufferID = nmhdr->idFrom;
	commonData.activeBufferID = bufferID;

    long whichScintilla = 0;
    npp(NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&whichScintilla);
    int view = (whichScintilla == 0) ? MAIN_VIEW : SUB_VIEW;

    //GetActiveViewForBuffer(bufferID);

    updateVirtualPanel(bufferID, view);
}

void beforeFileClose(const NMHDR* nmhdr) {
    if (plugin.startupOrShutdown) {
        return;
    }

    
}


void fileClosed(const NMHDR* nmhdr) {
    if (!commonData.isNppReady) return;
    if (plugin.startupOrShutdown) return;


    // If the file (buffer) is closed in one view but remains open in the other, or is moved from one view to the other,
    // we still get this notification. So we have to check to see if the buffer is still open in either view.
    auto position = npp(NPPM_GETPOSFROMBUFFERID, nmhdr->idFrom, 0);
    if (position == -1) /* file is no longer open in either view */ {

        onFileClosed(nmhdr->idFrom, MAIN_VIEW);
        onFileClosed(nmhdr->idFrom, SUB_VIEW);

        return;
    }
    

    toggleViewOfVFile(nmhdr->idFrom);
}


void fileOpened(const NMHDR* nmhdr) {
    //if (!commonData.annoy) return;
	if (!commonData.isNppReady) return; // Ensure Notepad++ is ready before showing message
    UINT_PTR bufferID = nmhdr->idFrom;
    std::wstring filepath = getFilePath(bufferID);
    //MessageBox(plugin.nppData._nppHandle, (L"You opened \"" + filepath + L"\", didn't you?").data(), L"VirtualFolders", 0);

    bufferActivated(nmhdr);
}

void fileRenamed(const NMHDR* nmhdr) {
    if (!commonData.isNppReady) return; // Ensure Notepad++ is ready before showing message
    UINT_PTR bufferID = nmhdr->idFrom;
    std::wstring filepath = getFilePath(bufferID);
	wchar_t* fullpathPtr = getFullPathFromBufferID(bufferID);
    if (!fullpathPtr) {
        return;
    }
    std::wstring fullpath(fullpathPtr);
    delete[] fullpathPtr; // Clean up the allocated memory

    onFileRenamed(nmhdr->idFrom, filepath, fullpath);
}

void fileSaved(const NMHDR* nmhdr) 
{
    if (!commonData.isNppReady) return;
    UINT_PTR bufferID = nmhdr->idFrom;
	//onFileSaved(bufferID);


    for (int view = MAIN_VIEW; view <= SUB_VIEW; ++view) {
        optional<VFile*> vFileOpt = commonData.rootVFolder.findFileByBufferID(bufferID, view);
        if (vFileOpt) {

            wstring wFullPath = getFilePath(bufferID);
            int len = WideCharToMultiByte(CP_UTF8, 0,
                wFullPath.c_str(), -1,
                nullptr, 0, nullptr, nullptr);
            std::string fullPath(len - 1, 0); // exclude the null terminator
            WideCharToMultiByte(CP_UTF8, 0,
                wFullPath.c_str(), -1,
                &fullPath[0], len, nullptr, nullptr);


            vFileOpt.value()->path = fullPath;

            std::filesystem::path file(fullPath);
            vFileOpt.value()->name = file.filename().string();


            vFileOpt.value()->isReadOnly = false; // After saving, the file is no longer read-only
            vFileOpt.value()->isEdited = false;   // After saving, the file is no longer edited
            vFileOpt.value()->backupFilePath = ""; // Clear backup path after saving
            vFileOpt.value()->isActive = true; // Mark as active on save
            changeTreeItemIcon(bufferID, view);
        }
	}
	writeJsonFile();
	

}

void readOnlyChanged(const NMHDR* nmhdr) {
    // This gets called when a file's read-only status changes
    // You can check the current buffer's read-only status if needed
	// bool isReadOnly = npp(NPPM_GETREADONLY, 0, 0);
    HWND bufferID = nmhdr->hwndFrom;
    //onReadOnlyChanged();
    int i = 0;

}

// Add this new notification handler for session loading
void sessionLoaded() {
    // This gets called when Notepad++ finishes loading the session
    // Sync vData with all loaded files
    //syncVDataWithOpenFilesNotification();
    LOG("Session loaded");
}

void nppReady() {
    // This gets called when Notepad++ is fully ready
    // Wait a bit for files to be loaded, then sync
    Sleep(100); // Small delay to ensure files are loaded
    //syncVDataWithOpenFilesNotification(); Buna gerek yok gibi

    commonData.isNppReady = true;
    
    // Small delay to ensure docking system is ready
    Sleep(200);

    // Restore tab selection if Virtual Folders tab was selected last time
    if (commonData.virtualFoldersTabSelected.get()) {
        // Switch to Virtual Folders tab
        npp(NPPM_DMMVIEWOTHERTAB, 0, reinterpret_cast<LPARAM>(L"Virtual Folders"));
    }
    
    syncVDataWithBufferIDs();
    loadLocalization();

    //SetTimer(virtualPanel, TREEVIEW_TIMER_ID, 3000, nullptr); // 3-second interval
}


void nppBeforeShutdown() {
    
}

void nppShutdown() {
    // This gets called when Notepad++ is shutting down
    
    // Save the current tab selection state
    // Check if Virtual Folders tab is currently visible/selected
    if (virtualPanelWnd && IsWindowVisible(virtualPanelWnd)) {
        commonData.virtualFoldersTabSelected = true;
    } else {
        commonData.virtualFoldersTabSelected = false;
    }

}


void modifyAll(const NMHDR* nmhdr) {
    // This message is sent once for each buffer ID in which text is modified; the same buffer could be visible in both views
    UINT_PTR bufferID = reinterpret_cast<UINT_PTR>(nmhdr->hwndFrom);
    intptr_t cdi1 = npp(NPPM_GETCURRENTDOCINDEX, 0, 0);
    intptr_t cdi2 = npp(NPPM_GETCURRENTDOCINDEX, 0, 1);
    bool visible1 = cdi1 < 0 ? false : bufferID == static_cast<UINT_PTR>(npp(NPPM_GETBUFFERIDFROMPOS, cdi1, 0));
    bool visible2 = cdi2 < 0 ? false : bufferID == static_cast<UINT_PTR>(npp(NPPM_GETBUFFERIDFROMPOS, cdi2, 1));
    if (visible1) {
        plugin.getScintillaPointers(plugin.nppData._scintillaMainHandle);
        // process changed visible main view
    }
    if (visible2) {
        plugin.getScintillaPointers(plugin.nppData._scintillaSecondHandle);
        // process changed visible secondary view
    }
}

wchar_t* getFullPathFromBufferID(UINT_PTR bufferID) {
    LRESULT posId = ::SendMessage(plugin.nppData._nppHandle, NPPM_GETPOSFROMBUFFERID, bufferID, 0);
    if (posId == -1) {
        return nullptr;
    }
    int len = (int)::SendMessage(plugin.nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, bufferID, 0);
    wchar_t* filePath = new wchar_t[len + 1];
    ::SendMessage(plugin.nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, bufferID, (LPARAM)filePath);

    return filePath;
}


/**
* return MAIN_VIEW: 0
*        SUB_VIEW: 1
*/
int GetActiveViewForBuffer(UINT_PTR bufferID)
{
    for (int view = SUB_VIEW; view >= MAIN_VIEW; --view)
    {
        int activeIndex = (int)npp(NPPM_GETCURRENTDOCINDEX, 0, (LPARAM)view);
        if (activeIndex != -1)
        {
            UINT_PTR activeBufID = (int)npp(NPPM_GETBUFFERIDFROMPOS, (WPARAM)activeIndex, (LPARAM)view);
            if (activeBufID == bufferID)
                return view; // this view has the active buffer
        }
    }
    return -1; // not active in either view
}