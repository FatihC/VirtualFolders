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
#include "resource.h"
#include "Shlwapi.h"
#include <iomanip>



extern NPP::ShortcutKey SKNextTab;  // Defined in Plugin.cpp


VFolder oldFolder;
VFolder newFolder;
int oldOrder;
int newOrder;


namespace {

    bool sendEmail();
    void openGitHubIssue();

   
    HWND beforeLink;
    HWND afterLink;

    HWND beforeEdit;
    HWND afterEdit;

    wstring subject = L"Tree Corruption Report";
    wstring emailAddress = L"virtualfoldersnotepad@gmail.com";



    DialogStretch stretch;
    config_rect placement("Corruption dialog placement");
    

    INT_PTR CALLBACK corruptionDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {

        switch (uMsg) {

        case WM_DESTROY:
            return TRUE;

        case WM_INITDIALOG:
        {
            stretch.setup(hwndDlg);
            /*placement.put(hwndDlg);*/

            config_rect::show(hwndDlg);  // centers dialog on owner client area, without saving position


            SetWindowText(hwndDlg, commonData.translator->getTextW("IDD_CORRUPTION_DIALOG").c_str());


            HWND warningText = GetDlgItem(hwndDlg, IDC_CORRUPTION_WARNING);
            wstring warningMessage = commonData.translator->getTextW("IDC_CORRUPTION_WARNING").c_str();
            SetWindowText(warningText, warningMessage.c_str());

            INITCOMMONCONTROLSEX icex{};
            icex.dwSize = sizeof(icex);
            icex.dwICC = ICC_LISTVIEW_CLASSES;
            InitCommonControlsEx(&icex);

            beforeLink = GetDlgItem(hwndDlg, IDC_CORRUPTION_SYSLINK_BEFORE);
            beforeEdit = GetDlgItem(hwndDlg, IDC_CORRUPTION_BEFORE_TREE);
            string oldJson = folderToMinJson(oldFolder);
            SetWindowText(beforeEdit, toWstring(oldJson).c_str());
            //ShowWindow(beforeEdit, SW_HIDE);


            afterLink = GetDlgItem(hwndDlg, IDC_CORRUPTION_SYSLINK_AFTER);
            afterEdit = GetDlgItem(hwndDlg, IDC_CORRUPTION_AFTER_TREE);
            string newJson = folderToMinJson(newFolder);
            SetWindowText(afterEdit, toWstring(newJson).c_str());
            //ShowWindow(afterEdit, SW_HIDE);

            SetWindowText(beforeLink, commonData.translator->getTextW("IDC_CORRUPTION_SYSLINK_BEFORE").c_str());
            SetWindowText(afterLink, commonData.translator->getTextW("IDC_CORRUPTION_SYSLINK_AFTER").c_str());

            HWND sendGithubBtn = GetDlgItem(hwndDlg, ID_CORRUPTION_SENDGITHUB);
            SetWindowText(sendGithubBtn, commonData.translator->getTextW("ID_CORRUPTION_SENDGITHUB").c_str());

            HWND sendEmailBtn = GetDlgItem(hwndDlg, ID_CORRUPTION_SENDEMAIL);
            SetWindowText(sendEmailBtn, commonData.translator->getTextW("ID_CORRUPTION_SENDEMAIL").c_str());

            HWND closeBtn = GetDlgItem(hwndDlg, ID_CLOSE);
            SetWindowText(closeBtn, commonData.translator->getTextW("ID_CLOSE").c_str());

            

            
            

            npp(NPPM_DARKMODESUBCLASSANDTHEME, NPP::NppDarkMode::dmfInit, hwndDlg);  // Include to support dark mode

            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
            case ID_CLOSE:
            case IDCANCEL:
                placement.get(hwndDlg);
                EndDialog(hwndDlg, 1);
                return TRUE;
            case ID_CORRUPTION_SENDEMAIL:
            {
                sendEmail();
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            case ID_CORRUPTION_SENDGITHUB:
                openGitHubIssue();
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            return FALSE;
        case WM_NOTIFY:
        {
            LPNMHDR pNMHDR = (LPNMHDR)lParam;

            if (pNMHDR->idFrom == IDC_CORRUPTION_SYSLINK_BEFORE && (pNMHDR->code == NM_CLICK || pNMHDR->code == NM_RETURN))
            {
                ShowWindow(beforeEdit, IsWindowVisible(beforeEdit) ? SW_HIDE : SW_SHOW);
                return TRUE;
            }

            if (pNMHDR->idFrom == IDC_CORRUPTION_SYSLINK_AFTER && (pNMHDR->code == NM_CLICK || pNMHDR->code == NM_RETURN))
            {
                ShowWindow(afterEdit, IsWindowVisible(afterEdit) ? SW_HIDE : SW_SHOW);
                return TRUE;
            }
        }
        break;
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO& mmi = *reinterpret_cast<MINMAXINFO*>(lParam);
            mmi.ptMinTrackSize.x = stretch.originalWidth();
            mmi.ptMinTrackSize.y = mmi.ptMaxTrackSize.y = stretch.originalHeight();
            return FALSE;
        }

        case WM_SIZE:
            stretch.adjust(IDC_CORRUPTION_BEFORE_TREE, 1)
                .adjust(IDC_CORRUPTION_AFTER_TREE, 1)
                .adjust(IDCANCEL, 0, 0, 1, 1)
                .adjust(IDOK, 0, 0, 1, 1);
            return FALSE;

        }

        return FALSE;
    }

    string getCompressedContent() {
        auto setAllNames = [&](auto&& self, VFolder& f) -> void {
            f.name = "xxx " + f.getOrder();
            f.path = "xxx " + f.getOrder();
            for (auto& file : f.fileList) {
                file.name = "xxx " + f.getOrder();
                file.path = "xxx " + f.getOrder();
                file.backupFilePath = "xxx " + f.getOrder();
            }
            for (auto& sub : f.folderList) self(self, sub);
            };


        oldFolder.vFolderSort();
        setAllNames(setAllNames, oldFolder);
        json oldDataJson = oldFolder;
        string oldJsonStr = oldDataJson.dump(0);
        vector<BYTE> oldCompressedVector = safeCompress(vector<BYTE>(oldJsonStr.begin(), oldJsonStr.end()));
        string oldCompressed = string(oldCompressedVector.begin(), oldCompressedVector.end());
        string oldBase64Str = base64_encode(oldCompressed);

        newFolder.vFolderSort();
        setAllNames(setAllNames, newFolder);
        json newDataJson = newFolder;
        string newJsonStr = newDataJson.dump(0);
        vector<BYTE> newCompressedVector = safeCompress(vector<BYTE>(newJsonStr.begin(), newJsonStr.end()));
        string newCompressed = string(newCompressedVector.begin(), newCompressedVector.end());
        string newBase64Str = base64_encode(newCompressed);

        string newLine = "\r\n";

        ostringstream bodyStream;
        bodyStream << "Old Order: " << oldOrder << newLine << newLine;
        bodyStream << "New Order: " << newOrder << newLine << newLine;
        bodyStream << "Old Root: " << oldBase64Str << newLine << newLine;
        bodyStream << "New Root: " << newBase64Str << newLine << newLine;

        string body = bodyStream.str();

        return body;
    }

    bool sendEmail() 
    {
        string body = getCompressedContent();

        wstring mailto = L"mailto:" + emailAddress + 
            L"?subject=" + subject +
            L"&body=" + UrlEncode(toWstring(body));

        ShellExecuteW(NULL, L"open", mailto.c_str(), NULL, NULL, SW_SHOWNORMAL);

        return true;
    }

    void openGitHubIssue()
    {
        string body = getCompressedContent();

        wstring issueBody =
            L"### Description\r\n"
            L"The tree structure appears corrupted.\r\n\r\n"
            L"### Compressed Data\r\n"
            L"```\r\n" + toWstring(body) + L"\r\n```";

        wstring url =
            L"https://github.com/FatihC/VirtualFolders/issues/new?"
            L"title=" + UrlEncode(subject) +
            L"&body=" + UrlEncode(issueBody);

        ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }


}

void showCorruptionDialog(VFolder hOldFolder, VFolder hNewFolder, int hOldOrder, int hNewOrder) {
    oldFolder = hOldFolder;
    newFolder = hNewFolder;
    oldOrder = hOldOrder;
    newOrder = hNewOrder;

    DialogBox(plugin.dllInstance, MAKEINTRESOURCE(IDD_CORRUPTION_DIALOG), plugin.nppData._nppHandle, corruptionDialogProc);
}