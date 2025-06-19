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

#include <commctrl.h>
#include "VData.h"
#pragma comment(lib, "comctl32.lib")


extern NPP::FuncItem menuDefinition[];  // Defined in Plugin.cpp
extern int menuItem_ToggleWatcher;      // Defined in Plugin.cpp

void writeJsonFile();
wchar_t* toWchar(const std::string& str);
void addFileToTree(const VFile vFile, HWND hTree, HTREEITEM hParent);
void addFolderToTree(const VFolder vFolder, HWND hTree, HTREEITEM hParent);


namespace {

HWND watcherPanel = 0;

Scintilla::Position location = -1;
Scintilla::Position terminal = -1;

DialogStretch stretch;

std::wstring jsonFilePath;
VData vData;

static HTREEITEM hDragItem = nullptr;
static HTREEITEM hDropTarget = nullptr;
static HIMAGELIST hDragImage = nullptr;
static bool isDragging = false;



void updateWatcherPanelUnconditional() {
    std::wstring text = GetDlgItemString(watcherPanel, IDC_WATCHER_TEXT);
    if (text.empty()) {
        SetDlgItemText(watcherPanel, IDC_WATCHER_RESULT, L"");
        location = -1;
        return;
    }
    plugin.getScintillaPointers();
    std::string s = fromWide(text);
    sci.TargetWholeDocument();
    sci.SetSearchFlags(Scintilla::FindOption::WholeWord);
    location = sci.SearchInTarget(s);
    if (location < 0) {
        SetDlgItemText(watcherPanel, IDC_WATCHER_RESULT, (L"\"" + text + L"\" not found.").data());
        return;
    }
    terminal = sci.TargetEnd();
    std::wstring linenum = std::to_wstring(sci.LineFromPosition(location) + 1);
    SetDlgItemText(watcherPanel, IDC_WATCHER_RESULT, (L"Found \"" + text + L"\" on line " + linenum + L".").data());
}


INT_PTR CALLBACK watcherDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM) {

    switch (uMsg) {

    case WM_DESTROY:
        watcherPanel = 0;
        return TRUE;

    case WM_INITDIALOG:
        stretch.setup(hwndDlg);
        npp(NPPM_MODELESSDIALOG, MODELESSDIALOGADD, hwndDlg);   // a docking dialog must be a modeless dialog
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDCANCEL:
            npp(NPPM_DMMHIDE, 0, hwndDlg);                      // make Esc key close the dialog
            return TRUE;
        case IDOK:
            SetFocus(plugin.currentScintilla());                // make Enter key return to active edit window
            if (location >= 0) {                                // and if the word was found
                plugin.getScintillaPointers();
                sci.SetSel(location, terminal);                 // select the first instance
            }
            return TRUE;
        case IDC_WATCHER_TEXT:
            if (HIWORD(wParam) == EN_CHANGE) {
                updateWatcherPanelUnconditional();
                return TRUE;
            }
        }
        return FALSE;

    case WM_SHOWWINDOW:
        if (wParam) updateWatcherPanelUnconditional();
        npp(NPPM_SETMENUITEMCHECK, menuDefinition[menuItem_ToggleWatcher]._cmdID, wParam ? 1 : 0);
        return FALSE;

    case WM_SIZE:
        stretch.adjust(IDC_WATCHER_TEXT, 1).adjust(IDC_WATCHER_RESULT, 1);
        return FALSE;
    
    }

    return FALSE;
}

}


void updateWatcherPanel() { if (watcherPanel && IsWindowVisible(watcherPanel)) updateWatcherPanelUnconditional(); }

void toggleWatcherPanel() {
    if (!watcherPanel) {
        watcherPanel = CreateDialog(plugin.dllInstance, MAKEINTRESOURCE(IDD_WATCHER), plugin.nppData._nppHandle, watcherDialogProc);
        NPP::tTbData dock;
        dock.hClient       = watcherPanel;
        dock.pszName       = L"Watcher (VFolders)";  // title bar text (caption in dialog is replaced)
        dock.dlgID         = menuItem_ToggleWatcher;          // zero-based position in menu to recall dialog at next startup
        dock.uMask         = DWS_DF_CONT_RIGHT;               // first time display will be docked at the right
        dock.pszModuleName = L"VFolders.dll";        // plugin module name
        npp(NPPM_DMMREGASDCKDLG, 0, &dock);
    }
    else if (IsWindowVisible(watcherPanel)) {
        npp(NPPM_DMMHIDE, 0, watcherPanel);
    }
    else {
        updateWatcherPanelUnconditional();
        npp(NPPM_DMMSHOW, 0, watcherPanel);
    }
}

std::vector<std::string> listOpenFiles() {
    Scintilla::EndOfLine eolMode = sci.EOLMode();
    std::wstring eol = eolMode == Scintilla::EndOfLine::Cr ? L"\r"
        : eolMode == Scintilla::EndOfLine::Lf ? L"\n"
        : L"\r\n";
    std::wstring filenames = commonData.heading.get() + eol;
    for (int view = 0; view < 2; ++view) if (npp(NPPM_GETCURRENTDOCINDEX, 0, view)) {
        size_t n = npp(NPPM_GETNBOPENFILES, 0, view + 1);
        for (size_t i = 0; i < n; ++i) filenames += getFilePath(npp(NPPM_GETBUFFERIDFROMPOS, i, view)) + eol;
    }

	// TODO: get real files and associate them with the open `files`. Return this list.
    return {};
}

void toggleWatcherPanelWithList() {
    if (!watcherPanel) {
        watcherPanel = CreateDialog(plugin.dllInstance, MAKEINTRESOURCE(106), plugin.nppData._nppHandle, watcherDialogProc);
        NPP::tTbData dock;
        HWND hTree = GetDlgItem(watcherPanel, 1023);

        
        TCHAR configDir[MAX_PATH];
        ::SendMessage(plugin.nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)configDir);
        jsonFilePath = std::wstring(configDir) + L"\\virtualfolders.json";

        // read JSON
        json vDataJson;
        std::ifstream(jsonFilePath) >> vDataJson;
        
        vData = vDataJson.get<VData>();
        

		
        //writeJsonFile();
        

		vDataSort(vData);
		int lastOrder = vData.folderList.empty() ? 0 : vData.folderList.back().order;
		lastOrder = std::max(lastOrder, vData.fileList.empty() ? 0 : vData.fileList.back().order);

        for (int i = 0; i <= lastOrder; i++) {
			auto vFile = findFileByOrder(vData.fileList, i);
            if (!vFile) {
                auto vFolder = findFolderByOrder(vData.folderList, i);
                if (vFolder) {
                    addFolderToTree(*vFolder, hTree, TVI_ROOT);
                }
            }
            else {
                addFileToTree(*vFile, hTree, TVI_ROOT);
            }

        }




        
        std::vector<std::string> openFileList = listOpenFiles();

        dock.hClient = watcherPanel;
        dock.pszName = L"Watcher (VFolders)";  // title bar text (caption in dialog is replaced)
        dock.dlgID = menuItem_ToggleWatcher;          // zero-based position in menu to recall dialog at next startup
        dock.uMask = DWS_DF_CONT_RIGHT;               // first time display will be docked at the right
        dock.pszModuleName = L"VFolders.dll";        // plugin module name
        npp(NPPM_DMMREGASDCKDLG, 0, &dock);
    }
    else if (IsWindowVisible(watcherPanel)) {
        npp(NPPM_DMMHIDE, 0, watcherPanel);
    }
    else {
        updateWatcherPanelUnconditional();
        npp(NPPM_DMMSHOW, 0, watcherPanel);
    }
}

void writeJsonFile() {
    json vDataJson = vData;
    std::ofstream(jsonFilePath) << vDataJson.dump(4); // Write JSON to file
}

wchar_t* toWchar(const std::string& str) {
	static wchar_t buffer[256];
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer, 256);
	return buffer;
}

void addFileToTree(const VFile vFile, HWND hTree, HTREEITEM hParent)
{
    wchar_t buffer[100];

    TVINSERTSTRUCT tvis = { 0 };
    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT;
    wcscpy_s(buffer, 100, toWchar(vFile.name));
    tvis.item.pszText = buffer;
    TreeView_InsertItem(hTree, &tvis);
}

void addFolderToTree(const VFolder vFolder, HWND hTree, HTREEITEM hParent)
{
    wchar_t buffer[100];

    TVINSERTSTRUCT tvis = { 0 };
    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT;
    wcscpy_s(buffer, 100, toWchar(vFolder.name));
    tvis.item.pszText = buffer;
    HTREEITEM hFolder = TreeView_InsertItem(hTree, &tvis);

	
    int lastOrder = vFolder.fileList.empty() ? 0 : vFolder.fileList.back().order;
	lastOrder = std::max(lastOrder, vFolder.folderList.empty() ? 0 : vFolder.folderList.back().order);
    for (int i = 0; i <= lastOrder; i++) {
        auto vFile = findFileByOrder(vFolder.fileList, i);
        if (vFile) {
            addFileToTree(*vFile, hTree, hFolder);
        }
        else {
			auto vSubFolder = findFolderByOrder(vFolder.folderList, i);
			if (vSubFolder) {
				addFolderToTree(*vSubFolder, hTree, hFolder);
			}
        }
    }


    if (vFolder.isExpanded) {
        TreeView_Expand(hTree, hFolder, TVE_EXPAND);
    }
}