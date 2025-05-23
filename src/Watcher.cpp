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
