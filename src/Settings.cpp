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

namespace {

    DialogStretch stretch;
    config_rect placement("Settings dialog placement");

INT_PTR CALLBACK settingsDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {

    switch (uMsg) {

    case WM_DESTROY:
        return TRUE;

    case WM_INITDIALOG:
    {
        stretch.setup(hwndDlg);
        placement.put(hwndDlg);

        // Option 1 is an edit control with a spin box allowing values from 5 - 5000
        SendDlgItemMessage(hwndDlg, IDC_SETTINGS_OPTION1_SPIN, UDM_SETRANGE32, 5, 5000);
        data.option1.put(hwndDlg, IDC_SETTINGS_OPTION1_SPIN);

        // Option 2 is a combo box that keeps a history
        data.option2.put(hwndDlg, IDC_SETTINGS_OPTION2);

        // Annoy is a simple check box
        data.annoy.put(hwndDlg, IDC_SETTINGS_ANNOY);

        // Heading is a simple text box
        data.heading.put(hwndDlg, IDC_SETTINGS_HEADING);

        // data.myPref is an enumeration
        switch (data.myPref) {
        case MyPreference::Bacon   : CheckRadioButton(hwndDlg, IDC_SETTINGS_PREFER_BACON, IDC_SETTINGS_PREFER_PIZZA, IDC_SETTINGS_PREFER_BACON   ); break;
        case MyPreference::IceCream: CheckRadioButton(hwndDlg, IDC_SETTINGS_PREFER_BACON, IDC_SETTINGS_PREFER_PIZZA, IDC_SETTINGS_PREFER_ICECREAM); break;
        case MyPreference::Pizza   : CheckRadioButton(hwndDlg, IDC_SETTINGS_PREFER_BACON, IDC_SETTINGS_PREFER_PIZZA, IDC_SETTINGS_PREFER_PIZZA   ); break;
        }

        npp(NPPM_DARKMODESUBCLASSANDTHEME, NPP::NppDarkMode::dmfInit, hwndDlg);  // Include to support dark mode

        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDCANCEL:
            placement.get(hwndDlg);
            EndDialog(hwndDlg, 1);
            return TRUE;
        case IDOK:
        {
            int option1;
            if (!data.option1.peek(option1, hwndDlg, IDC_SETTINGS_OPTION1_SPIN)) {
                ShowBalloonTip(hwndDlg, IDC_SETTINGS_OPTION1_SPIN, L"Option 1 must be a number between 5 and 5000.");
                return TRUE;
            }
            data.option1 = option1;
            data.option2.get(hwndDlg, IDC_SETTINGS_OPTION2);
            data.annoy.get(hwndDlg, IDC_SETTINGS_ANNOY);
            data.heading.get(hwndDlg, IDC_SETTINGS_HEADING);
            data.myPref = 
                IsDlgButtonChecked(hwndDlg, IDC_SETTINGS_PREFER_ICECREAM) == BST_CHECKED ? MyPreference::IceCream
              : IsDlgButtonChecked(hwndDlg, IDC_SETTINGS_PREFER_PIZZA   ) == BST_CHECKED ? MyPreference::Pizza
                                                                                         : MyPreference::Bacon;   
            placement.get(hwndDlg);
            EndDialog(hwndDlg, 0);
            return TRUE;
        }
        }
        return FALSE;

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO& mmi = *reinterpret_cast<MINMAXINFO*>(lParam);
        mmi.ptMinTrackSize.x = stretch.originalWidth();
        mmi.ptMinTrackSize.y = mmi.ptMaxTrackSize.y = stretch.originalHeight();
        return FALSE;
    }

    case WM_SIZE:
        stretch.adjust(IDC_SETTINGS_OPTION2, 1)
               .adjust(IDC_SETTINGS_HEADING, 1)
               .adjust(IDCANCEL, 0, 0, 1, 1)
               .adjust(IDOK, 0, 0, 1, 1);
        return FALSE;

    }

    return FALSE;
}

}

void showSettingsDialog() {
    DialogBox(plugin.dllInstance, MAKEINTRESOURCE(IDD_SETTINGS), plugin.nppData._nppHandle, settingsDialogProc);
}
