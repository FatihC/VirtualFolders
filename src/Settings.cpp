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



extern void setFontSize();
extern NPP::ShortcutKey SKNextTab;  // Defined in Plugin.cpp



namespace {

    HWND hListView;


    void applySettings(HWND hwndDlg);
    void FillFontCombo(HWND hwndCombo);
	void addListView(HWND hwndDlg);
	void addItemToListView(string actionName, string shortcut);


    config<bool> overrideShortcuts = { 
        "overrideShortcuts"  , 
        plugin.isShortcutOverridden 
    };

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


        INITCOMMONCONTROLSEX icex{};
        icex.dwSize = sizeof(icex);
        icex.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icex);



        // This is an edit control with a spin box allowing values from 5 - 5000
        SendDlgItemMessage(hwndDlg, IDC_SETTINGS_FONTSIZE_SPIN, UDM_SETRANGE32, 5, 5000);
        commonData.fontSize.put(hwndDlg, IDC_SETTINGS_FONTSIZE_SPIN);

        commonData.fontFamily.put(hwndDlg, IDC_SETTINGS_FONT_FAMILY_EDIT);

		HWND hCombo = GetDlgItem(hwndDlg, IDC_SETTINGS_FONT_FAMILY_EDIT);
		FillFontCombo(hCombo);
		if (hCombo && commonData.fontFamily.loaded) {
			int index = (int)SendMessage(hCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)commonData.fontFamily.value.c_str());
			if (index != CB_ERR) {
				SendMessage(hCombo, CB_SETCURSEL, index, 0);
			}
		}


		overrideShortcuts = plugin.isShortcutOverridden;
        overrideShortcuts.put(hwndDlg, IDC_SETTINGS_SHORTCUT_EDIT);

        addListView(hwndDlg);

        string actionNameNextTab = commonData.nativeTranslator->getCommand("44095");
        string actionNamePrevTab = commonData.nativeTranslator->getCommand("44096");

		string shortcutNextTab;
		string shortcutPrevTab;

        // BOOL NPPM_GETSHORTCUTBYCMDID(int cmdID, ShortcutKey* sk)
		bool res = (bool)npp(NPPM_GETSHORTCUTBYCMDID, IDM_VIEW_TAB_NEXT, (LPARAM) &SKNextTab);
        if (!res) {
            shortcutNextTab = commonData.nativeTranslator->getShortcut("44095");
            if (shortcutNextTab.empty()) {
                shortcutNextTab = "Ctrl+PageDown";
			}
        }
        else {
            shortcutNextTab = "";
            if (SKNextTab._isCtrl) shortcutNextTab += "Ctrl+";
            if (SKNextTab._isAlt) shortcutNextTab += "Alt+";
            if (SKNextTab._isShift) shortcutNextTab += "Shift+";
            if (SKNextTab._key != 0) {
                shortcutNextTab += fromWide(commonData.nativeTranslator->vkToKeyName(SKNextTab._key));
			}
        }
        res = (bool)npp(NPPM_GETSHORTCUTBYCMDID, IDM_VIEW_TAB_PREV, (LPARAM)&SKNextTab);
        if (!res) {
            shortcutPrevTab = commonData.nativeTranslator->getShortcut("44096");
            if (shortcutPrevTab.empty()) {
                shortcutPrevTab = "Ctrl+PageUp";
			}
        }
        else {
            shortcutPrevTab = "";
            if (SKNextTab._isCtrl) shortcutPrevTab += "Ctrl+";
            if (SKNextTab._isAlt) shortcutPrevTab += "Alt+";
            if (SKNextTab._isShift) shortcutPrevTab += "Shift+";
            if (SKNextTab._key != 0) {
                shortcutPrevTab += fromWide(commonData.nativeTranslator->vkToKeyName(SKNextTab._key));
            }
        }
        


        addItemToListView(actionNameNextTab, shortcutNextTab);
        addItemToListView(actionNamePrevTab, shortcutPrevTab);


        npp(NPPM_DARKMODESUBCLASSANDTHEME, NPP::NppDarkMode::dmfInit, hwndDlg);  // Include to support dark mode

        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDCANCEL:
            placement.get(hwndDlg);
            EndDialog(hwndDlg, 1);
            return TRUE;
        case IDAPPLY:
            applySettings(hwndDlg);
            return TRUE;
        case IDOK:
        {
            applySettings(hwndDlg);
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
        stretch.adjust(IDC_SETTINGS_FONT_FAMILY_EDIT, 1)
               .adjust(IDCANCEL, 0, 0, 1, 1)
               .adjust(IDOK, 0, 0, 1, 1);
        return FALSE;

    }

    return FALSE;
}

void applySettings(HWND hwndDlg) {
    int fontSize;
    if (!commonData.fontSize.peek(fontSize, hwndDlg, IDC_SETTINGS_FONTSIZE_SPIN)) {
        ShowBalloonTip(hwndDlg, IDC_SETTINGS_FONTSIZE_SPIN, L"Font size must be a number between 5 and 5000.");
        return;
    }
    commonData.fontSize = fontSize;


    wstring fontFamily = commonData.fontFamily.get(hwndDlg, IDC_SETTINGS_FONT_FAMILY_EDIT);
	commonData.fontFamily = fontFamily;

    setFontSize();

    HWND hCombo = GetDlgItem(hwndDlg, IDC_SETTINGS_FONT_FAMILY_EDIT);
    if (hCombo && fontFamily.size() > 0) {
        int index = (int)SendMessage(hCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)fontFamily.c_str());
        if (index != CB_ERR) {
            SendMessage(hCombo, CB_SETCURSEL, index, 0);
        }
    }

    overrideShortcuts.get(hwndDlg, IDC_SETTINGS_SHORTCUT_EDIT);
    if (overrideShortcuts != plugin.isShortcutOverridden) {
	    plugin.isShortcutOverridden = !overrideShortcuts; // temporary. it will reverse in toggle...
        toggleShortcutOverride();
	}

    //placement.get(hwndDlg);
}

struct FontEnumData {
    HWND hwndCombo;
    std::set<std::wstring> fontNames;
};


int CALLBACK EnumFontFamExProc(const LOGFONT* lpelfe, const TEXTMETRIC* lpntme, DWORD FontType, LPARAM lParam)
{
    FontEnumData* data = reinterpret_cast<FontEnumData*>(lParam);
    std::wstring name(lpelfe->lfFaceName);
    if (data->fontNames.insert(name).second) {
        SendMessage(data->hwndCombo, CB_ADDSTRING, 0, (LPARAM)lpelfe->lfFaceName);
    }
    return 1; // continue enumeration
}

void FillFontCombo(HWND hwndCombo)
{
    SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0); // Clear previous items

    FontEnumData data = { hwndCombo };
    LOGFONT lf = { 0 };
    lf.lfCharSet = DEFAULT_CHARSET;
    HDC hdc = GetDC(NULL);
    EnumFontFamiliesEx(hdc, &lf, (FONTENUMPROC)EnumFontFamExProc, (LPARAM)&data, 0);
    ReleaseDC(NULL, hdc);
}

void addListView(HWND hwndDlg)
{
    hListView = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEW,
        L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        40, 170, // x, y
        330, 100, // width, height
        hwndDlg,
        (HMENU)IDC_SETTINGS_ACTION_LIST,
        plugin.dllInstance,
        NULL
    );

    ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    HFONT hFont = CreateFontW(
        18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS,
        L"Segoe UI"   // font name
    );

    SendMessage(hListView, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));

    // Column headers (do once)
    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    col.cx = 215;
    col.pszText = toWchar("Action");
    ListView_InsertColumn(hListView, 0, &col);

    col.cx = 110;
    col.pszText = toWchar("Shortcut");
    ListView_InsertColumn(hListView, 1, &col);
}

void addItemToListView(string actionName, string shortcut)
{
    // Adding a row (item)
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = 0;               // row index
    item.iSubItem = 0;            // first column
    item.pszText = toWchar(actionName);  // action name
    ListView_InsertItem(hListView, &item);

    // Add text to second column (subitem)
    item.iSubItem = 1;
    item.pszText = toWchar(shortcut);
    ListView_SetItem(hListView, &item);

}

}

void showSettingsDialog() {
    DialogBox(plugin.dllInstance, MAKEINTRESOURCE(IDD_SETTINGS), plugin.nppData._nppHandle, settingsDialogProc);
}