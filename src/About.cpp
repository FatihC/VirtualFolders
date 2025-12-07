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

#include <chrono>
#include "CommonData.h"
#include "resource.h"
#include "Shlwapi.h"


static bool isArmSystem();


INT_PTR CALLBACK aboutDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {

    static wstring version;  // Once filled in, we don't need to get this information again if About is called again.

    switch (uMsg) {

    case WM_DESTROY:
        return TRUE;

    case WM_INITDIALOG:
        {
            config_rect::show(hwndDlg);  // centers dialog on owner client area, without saving position

            SetDlgItemText(hwndDlg, IDC_ABOUT_DESCRIPTION, commonData.translator->getTextW("IDC_ABOUT_DESCRIPTION").c_str());

            if (version.empty()) {
                version = commonData.translator->getTextW("ID_ABOUT_VERSION");
                version += GetPluginVersion(plugin.dllInstance);

                if constexpr (sizeof(size_t) == 8) {
                    if (isArmSystem()) version += L" (ARM64)";
                    else version += L" (x64)";
                }
                else if constexpr (sizeof(size_t) == 4) version += L" (x86)";
                version += L".\n";

                LOG("versionNameString: {}", fromWide(version));


                
                auto pidh = reinterpret_cast<IMAGE_DOS_HEADER*>(plugin.dllInstance);
                auto pnth = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<char*>(pidh) + pidh->e_lfanew);
                auto timepoint = chrono::sys_seconds(chrono::seconds(pnth->FileHeader.TimeDateStamp));
                version += format(L"Build time: {0:%Y} {0:%b} {0:%d} at {0:%H}:{0:%M}:{0:%S} UTC.", timepoint);

                version += L"\n\nby Fatih COÞKUN";
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
                wstring path(n, 0);
                SendMessage(plugin.nppData._nppHandle, NPPM_GETPLUGINHOMEPATH, n + 1, reinterpret_cast<LPARAM>(path.data()));

                static wstring changes = path + L"\\VirtualFolders\\CHANGELOG.md";
                if (PathFileExists(changes.data()) == TRUE) {
                    PostMessage(plugin.nppData._nppHandle, NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(changes.data()));
                    PostMessage(plugin.nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_EDIT_TOGGLEREADONLY);
                }
                static wstring license = path + L"\\VirtualFolders\\LICENSE.txt";
                if (PathFileExists(license.data()) == TRUE) {
                    PostMessage(plugin.nppData._nppHandle, NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(license.data()));
                    PostMessage(plugin.nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_EDIT_TOGGLEREADONLY);
                }
                static wstring readme = path + L"\\VirtualFolders\\README.md";
                if (PathFileExists(readme.data()) == TRUE) {
                    PostMessage(plugin.nppData._nppHandle, NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(readme.data()));
                    PostMessage(plugin.nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_EDIT_TOGGLEREADONLY);
                }

                PostMessage(plugin.nppData._nppHandle, NPPM_SWITCHTOFILE, 0, reinterpret_cast<LPARAM>(changes.data()));
            }
            EndDialog(hwndDlg, 0);
            return TRUE;
        }
        case WM_NOTIFY:
        {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (pnmh->idFrom == IDC_ABOUT_GITHUB_LINK && pnmh->code == NM_CLICK)
            {
                PNMLINK pNMLink = (PNMLINK)lParam;
                ShellExecuteW(nullptr, L"open", pNMLink->item.szUrl, nullptr, nullptr, SW_SHOWNORMAL);
            }
            else if (pnmh->idFrom == IDC_ABOUT_GITHUB_LINK && pnmh->code == NM_RETURN)
            {
                PNMLINK pNMLink = (PNMLINK)lParam;
                ShellExecuteW(nullptr, L"open", pNMLink->item.szUrl, nullptr, nullptr, SW_SHOWNORMAL);
            }
        }
        break;
        break;

    }
    return FALSE;
}

static bool isArmSystem() {
    // Prefer IsWow64Process2 when available (Windows 10+). It returns native machine type.
    using LPFN_ISWOW64PROCESS2 = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);
    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    if (hKernel) {
        auto fn = reinterpret_cast<LPFN_ISWOW64PROCESS2>(GetProcAddress(hKernel, "IsWow64Process2"));
        if (fn) {
            USHORT processMachine = 0, nativeMachine = 0;
            if (fn(GetCurrentProcess(), &processMachine, &nativeMachine)) {
                if (nativeMachine == IMAGE_FILE_MACHINE_ARM64 || nativeMachine == IMAGE_FILE_MACHINE_ARMNT) {
                    return true;
                }
                return false;
            }
        }
    }

    // Fallback: GetNativeSystemInfo and examine processor architecture.
    SYSTEM_INFO si{};
    using LPFN_GETNATIVESYSTEMINFO = void(WINAPI*)(LPSYSTEM_INFO);
    auto gns = reinterpret_cast<LPFN_GETNATIVESYSTEMINFO>(GetProcAddress(hKernel, "GetNativeSystemInfo"));
    if (gns) gns(&si);
    else GetSystemInfo(&si);

    if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64 ||
        si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM) {
        return true;
    }

    return false;
}


void showAboutDialog() {
    DialogBox(plugin.dllInstance, MAKEINTRESOURCE(IDD_ABOUT), plugin.nppData._nppHandle, aboutDialogProc);
}
