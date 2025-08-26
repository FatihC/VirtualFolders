#include "CommonData.h"
#include "resource.h"
#include "Shlwapi.h"
#include <CommCtrl.h>
#include <string>
#include "Host/Notepad_plus_msgs.h"
#include "Host/menuCmdID.h"

namespace {

    static DialogStretch renameStretch;
    config_rect placement("Rename dialog placement");
    
    // Global variables to store the item being renamed
    VBase* itemToRename = nullptr;
    HTREEITEM treeItemToRename = nullptr;
    HWND hTreeToUpdate = nullptr;

    INT_PTR CALLBACK renameDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {

        switch (uMsg) {

        case WM_DESTROY:
            return TRUE;

        case WM_INITDIALOG:
        {
            renameStretch.setup(hwndDlg);
            placement.put(hwndDlg);

            // Set the old name in the read-only field
            if (itemToRename) {
                // Convert regular string to wide string for display
                int len = MultiByteToWideChar(CP_UTF8, 0, itemToRename->name.c_str(), -1, nullptr, 0);
                if (len > 0) {
                    std::wstring oldName(len - 1, L'\0');
                    MultiByteToWideChar(CP_UTF8, 0, itemToRename->name.c_str(), -1, &oldName[0], len);
                    SetDlgItemText(hwndDlg, IDC_RENAME_OLDNAME, oldName.c_str());
                    
                    // Set the current name as the default value in the edit field
                    SetDlgItemText(hwndDlg, IDC_RENAME_NEWNAME, oldName.c_str());
                }
                
                // Select all text in the edit field for easy editing
                SendDlgItemMessage(hwndDlg, IDC_RENAME_NEWNAME, EM_SETSEL, 0, -1);
                SetFocus(GetDlgItem(hwndDlg, IDC_RENAME_NEWNAME));
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
                // Get the new name from the edit control
                wchar_t newNameBuffer[256];
                GetDlgItemText(hwndDlg, IDC_RENAME_NEWNAME, newNameBuffer, sizeof(newNameBuffer) / sizeof(wchar_t));
                
                // Convert wide string to regular string
                std::string newName;
                int len = WideCharToMultiByte(CP_UTF8, 0, newNameBuffer, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    newName.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, newNameBuffer, -1, &newName[0], len, nullptr, nullptr);
                }
                
                // Trim whitespace
                newName.erase(0, newName.find_first_not_of(" \t\r\n"));
                newName.erase(newName.find_last_not_of(" \t\r\n") + 1);
                
                // Validate the new name
                if (newName.empty()) {
                    MessageBox(hwndDlg, L"Name cannot be empty.", L"Rename Error", MB_OK | MB_ICONERROR);
                    return TRUE;
                }
                
                // Update the item's name
                if (itemToRename) {
                    itemToRename->name = newName;
                    
                    // Update the tree item text
                    if (treeItemToRename && hTreeToUpdate) {
                        TVITEM tvi = { 0 };
                        tvi.mask = TVIF_TEXT;
                        tvi.hItem = treeItemToRename;
                        tvi.pszText = newNameBuffer; // Use the wide string version
                        TreeView_SetItem(hTreeToUpdate, &tvi);

                        VFolder* vFolder = dynamic_cast<VFolder*>(itemToRename);
                        if (vFolder) {
                            // TODO: write json file
                        }
                    }
                }
                
                placement.get(hwndDlg);
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            }
            return FALSE;

        case WM_GETMINMAXINFO:
        {
            MINMAXINFO& mmi = *reinterpret_cast<MINMAXINFO*>(lParam);
            mmi.ptMinTrackSize.x = renameStretch.originalWidth();
            mmi.ptMinTrackSize.y = mmi.ptMaxTrackSize.y = renameStretch.originalHeight();
            return FALSE;
        }

        case WM_SIZE:
            renameStretch.adjust(IDC_RENAME_NEWNAME, 1)
                   .adjust(IDC_RENAME_OLDNAME, 1)
                   .adjust(IDCANCEL, 0, 0, 1, 1)
                   .adjust(IDOK, 0, 0, 1, 1);
            return FALSE;

        }

        return FALSE;
    }

}

void showRenameDialog(VBase* item, HTREEITEM treeItem, HWND hTree) {
    // Store the item to be renamed globally
    itemToRename = item;
    treeItemToRename = treeItem;
    hTreeToUpdate = hTree;
    
    DialogBox(plugin.dllInstance, MAKEINTRESOURCE(IDD_RENAME), plugin.nppData._nppHandle, renameDialogProc);
    
    // Clear the global variables
    itemToRename = nullptr;
    treeItemToRename = nullptr;
    hTreeToUpdate = nullptr;
}
