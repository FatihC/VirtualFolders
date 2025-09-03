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
#include "ProcessCommands.h"
#include "DocumentListListener.h"



extern void updateStatusDialog();
extern void updateWatcherPanel(UINT_PTR bufferID);
extern void onBeforeFileClosed(int pos);
extern void onFileRenamed(int pos, wstring filepath, wstring fullpath);
void scnSavePointEvent(UINT_PTR bufferID, bool isSavePoint);
extern void changeTreeItemIcon(UINT_PTR bufferID);
extern void docOrderChanged(UINT_PTR bufferID, int newIndex, wchar_t* filePath);



// External variables
extern HWND watcherPanel;


wchar_t* getFullPathFromBufferID(UINT_PTR bufferID);


void scnModified(const Scintilla::NotificationData* scnp) {
    using Scintilla::FlagSet;
    if (FlagSet(scnp->modificationType, Scintilla::ModificationFlags::InsertText)) ++commonData.insertsCounted;
    else if (FlagSet(scnp->modificationType, Scintilla::ModificationFlags::DeleteText)) ++commonData.deletesCounted;
    else return;
    //updateStatusDialog();
    //updateWatcherPanel();
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
    changeTreeItemIcon(bufferID);
}


void scnUpdateUI(const Scintilla::NotificationData* /* scnp */) {
}


void scnZoom(const Scintilla::NotificationData* /* scnp */) {
}


void bufferActivated(const NMHDR* nmhdr) {
    UINT_PTR bufferID = nmhdr->idFrom;
	commonData.activeBufferID = bufferID;
    updateWatcherPanel(bufferID);
}

void beforeFileClose(const NMHDR* nmhdr) {
    if (plugin.startupOrShutdown) {
        return;
    }
    auto position = npp(NPPM_GETPOSFROMBUFFERID, nmhdr->idFrom, 0);
    if (position == -1) {
        return;
    }

    // TODO: remove file from vData
    onBeforeFileClosed(position);
}


void fileClosed(const NMHDR* nmhdr) {
    if (!commonData.annoy) return;
    // If the file (buffer) is closed in one view but remains open in the other, or is moved from one view to the other,
    // we still get this notification. So we have to check to see if the buffer is still open in either view.
    auto position = npp(NPPM_GETPOSFROMBUFFERID, nmhdr->idFrom, 0);
    if (position == -1) /* file is no longer open in either view */ {
        //MessageBox(plugin.nppData._nppHandle, L"You closed a file, didn't you?", L"VFolders", 0);
    }
}


void fileOpened(const NMHDR* nmhdr) {
    //if (!commonData.annoy) return;
	if (!commonData.isNppReady) return; // Ensure Notepad++ is ready before showing message
    UINT_PTR bufferID = nmhdr->idFrom;
    std::wstring filepath = getFilePath(bufferID);
    //MessageBox(plugin.nppData._nppHandle, (L"You opened \"" + filepath + L"\", didn't you?").data(), L"VFolders", 0);

    //TODO: have to implement this. just in case multiple files opened
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

    auto position = npp(NPPM_GETPOSFROMBUFFERID, nmhdr->idFrom, 0);
    if (position == -1) {
        return;
    }
    onFileRenamed(position, filepath, fullpath);
}

void docOrderChanged(const NMHDR* nmhdr) {
    UINT_PTR bufferID = nmhdr->idFrom;
    int newIndex = (int)nmhdr->hwndFrom;  // overloaded use

    wchar_t* filePath = getFullPathFromBufferID(bufferID);
    docOrderChanged(bufferID, newIndex, filePath);
}

// Add this new notification handler for session loading
void sessionLoaded() {
    // This gets called when Notepad++ finishes loading the session
    // Sync vData with all loaded files
    //syncVDataWithOpenFilesNotification();
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
        // Hook Document List
    }
    
    //hookDocList(plugin.nppData._nppHandle);
    
    SetTimer(watcherPanel, 1, 1000, nullptr); // 1-second interval

}

void nppShutdown() {
    // This gets called when Notepad++ is shutting down
    
    // Save the current tab selection state
    // Check if Virtual Folders tab is currently visible/selected
    if (watcherPanel && IsWindowVisible(watcherPanel)) {
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
