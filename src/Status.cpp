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
extern int menuItem_ToggleStatus;       // Defined in Plugin.cpp


namespace {

HWND statusDialog = 0;

INT_PTR CALLBACK statusDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM) {

    switch (uMsg) {

    case WM_DESTROY:
        return TRUE;

    case WM_INITDIALOG:
    {
        RECT rcNpp, rcDlg;
        GetWindowRect(plugin.nppData._nppHandle, &rcNpp);
        GetWindowRect(hwndDlg, &rcDlg);
        SetWindowPos(hwndDlg, HWND_TOP, (rcNpp.left + rcNpp.right + rcDlg.left - rcDlg.right) / 2,
            (rcNpp.top + rcNpp.bottom + rcDlg.top - rcDlg.bottom) / 2, 0, 0, SWP_NOSIZE);

        npp(NPPM_MODELESSDIALOG, MODELESSDIALOGADD, hwndDlg);
        npp(NPPM_DARKMODESUBCLASSANDTHEME, NPP::NppDarkMode::dmfInit, hwndDlg);  // Include to support dark mode
        npp(NPPM_SETMENUITEMCHECK, menuDefinition[menuItem_ToggleStatus]._cmdID, 1);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDCANCEL:
            npp(NPPM_MODELESSDIALOG, MODELESSDIALOGREMOVE, hwndDlg);
            npp(NPPM_SETMENUITEMCHECK, menuDefinition[menuItem_ToggleStatus]._cmdID, 0);
            DestroyWindow(hwndDlg);
            statusDialog = 0;
            return TRUE;
        case IDOK:
            SetFocus(plugin.currentScintilla());                // make Enter key return to active edit window
            return TRUE;
        }
        return FALSE;
    }

    return FALSE;
}

}


void updateStatusDialog() {
    if (!statusDialog) return;
    SetDlgItemInt(statusDialog, IDC_STATUS_INSERTS, data.insertsCounted, true);
    SetDlgItemInt(statusDialog, IDC_STATUS_DELETES, data.deletesCounted, true);
}

void toggleStatusDialog() {
    if (statusDialog) SendMessage(statusDialog, WM_COMMAND, IDCANCEL, 0);
    else {
        statusDialog = CreateDialog(plugin.dllInstance, MAKEINTRESOURCE(IDD_STATUS), plugin.nppData._nppHandle, statusDialogProc);
        updateStatusDialog();
        ShowWindow(statusDialog, SW_NORMAL);
    }
}
