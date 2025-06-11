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


namespace {

HWND watcherPanel = 0;

Scintilla::Position location = -1;
Scintilla::Position terminal = -1;

DialogStretch stretch;

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
    return {};
}

void toggleWatcherPanelWithList() {
    if (!watcherPanel) {
        watcherPanel = CreateDialog(plugin.dllInstance, MAKEINTRESOURCE(106), plugin.nppData._nppHandle, watcherDialogProc);
        NPP::tTbData dock;
        HWND hTree = GetDlgItem(watcherPanel, 1023);

        wchar_t buffer[100];

        TCHAR configDir[MAX_PATH];
        ::SendMessage(plugin.nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)configDir);
        std::wstring jsonFilePath = std::wstring(configDir) + L"\\virtualfolders.json";


        // Now use configDir for your settings file


		// TODO: Read json file and populate the tree view with items from the list of open files.
        VData vData{};
        VFolder vFolder{};
		VFile vFile{"XMLTools.ini", "C:\\Users\\FatihCoskun\\AppData\\Roaming\\Notepad++\\plugins\\config"};
		vFolder.fileList.push_back(vFile);
		vData.folderList.push_back(vFolder);
		vData.fileList.push_back(vFile);

        json vJson = vData;
		std::ofstream(jsonFilePath) << vJson.dump(4); // Write JSON to file

        // Example: create and write JSON
        //json j;
        //j["name"] = "VFolders";
        //j["version"] = 1;
        //std::ofstream(jsonFilePath) << j.dump(4);

        //// Example: read JSON
        //json j2;
        //std::ifstream(jsonFilePath) >> j2;
        //std::string name = j2["name"];
        

        // Example: Add root and child items
        TVINSERTSTRUCT tvis = { 0 };
        tvis.hParent = TVI_ROOT; // Add to root
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT;
        wcscpy_s(buffer, 100, L"Dosyalar");
        tvis.item.pszText = buffer;
        HTREEITEM hRoot = TreeView_InsertItem(hTree, &tvis);

        // Add a child to the root
        tvis.hParent = hRoot;
        wcscpy_s(buffer, 100, L"Child Item");
        tvis.item.pszText = buffer;
        TreeView_InsertItem(hTree, &tvis);


        
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
