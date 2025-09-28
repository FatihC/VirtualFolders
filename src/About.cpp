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

#include <chrono>
#include "CommonData.h"
#include "resource.h"
#include "Shlwapi.h"


INT_PTR CALLBACK aboutDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM) {

    static std::wstring version;  // Once filled in, we don't need to get this information again if About is called again.

    switch (uMsg) {

    case WM_DESTROY:
        return TRUE;

    case WM_INITDIALOG:
        {
            config_rect::show(hwndDlg);  // centers dialog on owner client area, without saving position

            if (version.empty()) {
                VS_FIXEDFILEINFO* fixedFileInfo;
                UINT size;
                HRSRC hRes = FindResource(plugin.dllInstance, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
                if (hRes) {
                    HGLOBAL hGlobal = LoadResource(plugin.dllInstance, hRes);
                    if (hGlobal) {
                        void* pVersionInfo = LockResource(hGlobal);
                        if (pVersionInfo) {
                            if (VerQueryValue(pVersionInfo, L"\\", reinterpret_cast<void**>(&fixedFileInfo), &size) && size > 0) {
                                int versionPart1 = 0x0000ffff & (fixedFileInfo->dwProductVersionMS >> 16);
                                int versionPart2 = 0x0000ffff & fixedFileInfo->dwProductVersionMS;
                                int versionPart3 = 0x0000ffff & (fixedFileInfo->dwProductVersionLS >> 16);
                                int versionPart4 = 0x0000ffff & fixedFileInfo->dwProductVersionLS;
                                version = L"This is version " + std::to_wstring(versionPart1)
                                        + L"." + std::to_wstring(versionPart2);
                                if (versionPart3 || versionPart4) version += L"." + std::to_wstring(versionPart3);
                                if (versionPart4) version += L"." + std::to_wstring(versionPart4);
                                if constexpr (sizeof(size_t) == 8) version += L" (x64)";
                                else if constexpr (sizeof(size_t) == 4) version += L" (x86)";
                                version += L".\n\n";
                            }
                        }
                    }
                }
                auto pidh = reinterpret_cast<IMAGE_DOS_HEADER*>(plugin.dllInstance);
                auto pnth = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<char*>(pidh) + pidh->e_lfanew);
                auto timepoint = std::chrono::sys_seconds(std::chrono::seconds(pnth->FileHeader.TimeDateStamp));
                version += std::format(L"Build time: {0:%Y} {0:%b} {0:%d} at {0:%H}:{0:%M}:{0:%S} UTC.", timepoint);

                version += L"\n\nby Fatih COÞKUN\n";
            }

            SetDlgItemText(hwndDlg, IDC_ABOUT_VERSION, version.data());
            SendDlgItemMessage(hwndDlg, IDC_ABOUT_MORE, BCM_SETNOTE, 0, reinterpret_cast<LPARAM>(
                L"Show change log, license and other information."));

            npp(NPPM_DARKMODESUBCLASSANDTHEME, NPP::NppDarkMode::dmfInit, hwndDlg);  // Include to support dark mode
            return TRUE;
        }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDCANCEL:
        case IDOK:
            EndDialog(hwndDlg, 0);
            return TRUE;
        case IDC_ABOUT_MORE:
            {
                auto n = SendMessage(plugin.nppData._nppHandle, NPPM_GETPLUGINHOMEPATH, 0, 0);
                std::wstring path(n, 0);
                SendMessage(plugin.nppData._nppHandle, NPPM_GETPLUGINHOMEPATH, n + 1, reinterpret_cast<LPARAM>(path.data()));

                static std::wstring changes = path + L"\\VFolders\\CHANGELOG.md";
                if (PathFileExists(changes.data()) == TRUE) {
                    PostMessage(plugin.nppData._nppHandle, NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(changes.data()));
                    PostMessage(plugin.nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_EDIT_TOGGLEREADONLY);
                }
                static std::wstring license = path + L"\\VFolders\\LICENSE.txt";
                if (PathFileExists(license.data()) == TRUE) {
                    PostMessage(plugin.nppData._nppHandle, NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(license.data()));
                    PostMessage(plugin.nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_EDIT_TOGGLEREADONLY);
                }
                static std::wstring readme = path + L"\\VFolders\\README.md";
                if (PathFileExists(readme.data()) == TRUE) {
                    PostMessage(plugin.nppData._nppHandle, NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(readme.data()));
                    PostMessage(plugin.nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_EDIT_TOGGLEREADONLY);
                }

                PostMessage(plugin.nppData._nppHandle, NPPM_SWITCHTOFILE, 0, reinterpret_cast<LPARAM>(changes.data()));
            }
            EndDialog(hwndDlg, 0);
            return TRUE;
        }
        break;

    }
    return FALSE;
}


void showAboutDialog() {
    DialogBox(plugin.dllInstance, MAKEINTRESOURCE(IDD_ABOUT), plugin.nppData._nppHandle, aboutDialogProc);
}
