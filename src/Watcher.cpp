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
#include <iostream>

#include <commctrl.h>
#include "VData.h"
#pragma comment(lib, "comctl32.lib")

#include <windowsx.h>

extern NPP::FuncItem menuDefinition[];  // Defined in Plugin.cpp
extern int menuItem_ToggleWatcher;      // Defined in Plugin.cpp

void writeJsonFile();
wchar_t* toWchar(const std::string& str);
void addFileToTree(const VFile vFile, HWND hTree, HTREEITEM hParent);
void addFolderToTree(const VFolder vFolder, HWND hTree, HTREEITEM hParent);

#define ID_TREE_DELETE 40001

void updateTreeColorsExternal(HWND hTree);

HWND watcherPanel = 0;

namespace {

Scintilla::Position location = -1;
Scintilla::Position terminal = -1;

DialogStretch stretch;

std::wstring jsonFilePath;
VData vData;

static HTREEITEM hDragItem = nullptr;
static HTREEITEM hDropTarget = nullptr;
static HIMAGELIST hDragImage = nullptr;
static bool isDragging = false;


int idxFolder;
int idxFile;

std::string new_line = "\n";

struct InsertionMark {
    HTREEITEM hItem = nullptr;
    bool above = false;
    RECT rect = {0,0,0,0};
    bool valid = false;
    // Store actual line coordinates for reliable erasing
    int lineY = 0;
    int lineLeft = 0;
    int lineRight = 0;
};

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


INT_PTR CALLBACK watcherDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hTree = nullptr;
    switch (uMsg) {
    case WM_DESTROY:
        watcherPanel = 0;
        return TRUE;

    case WM_INITDIALOG:
        stretch.setup(hwndDlg);
        npp(NPPM_MODELESSDIALOG, MODELESSDIALOGADD, hwndDlg);
        hTree = GetDlgItem(hwndDlg, IDC_TREE1);
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

    case WM_NOTIFY: {
        LPNMHDR nmhdr = (LPNMHDR)lParam;
        if (nmhdr->idFrom == IDC_TREE1) {
            LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
            switch (pnmtv->hdr.code) {
            case TVN_BEGINDRAG: {
                hDragItem = pnmtv->itemNew.hItem;
                hDragImage = TreeView_CreateDragImage(hTree, hDragItem);
                if (hDragImage) {
                    POINT pt = pnmtv->ptDrag;
                    ClientToScreen(hTree, &pt);
                    ImageList_BeginDrag(hDragImage, 0, 0, 0);
                    ImageList_DragEnter(NULL, pt.x, pt.y);
                    SetCapture(hwndDlg);
                    isDragging = true;
                }
                return TRUE;
            }
            }
        }
        break;
    }
    case WM_MOUSEMOVE: {
        if (isDragging && hDragImage) {
            POINT pt;
            GetCursorPos(&pt);
            ImageList_DragMove(pt.x, pt.y);
            TVHITTESTINFO hitTest = { 0 };
            hitTest.pt = pt;
            ScreenToClient(hTree, &hitTest.pt);
            hDropTarget = TreeView_HitTest(hTree, &hitTest);
            TreeView_SelectDropTarget(hTree, hDropTarget);
            return TRUE;
        }
        break;
    }
    case WM_LBUTTONUP: {
        if (isDragging) {
            ReleaseCapture();
            ImageList_EndDrag();
            ImageList_Destroy(hDragImage);
            hDragImage = nullptr;
            isDragging = false;
            if (hDragItem && hDropTarget && hDragItem != hDropTarget) {
                TreeView_SelectDropTarget(hTree, nullptr);
                // TODO: Move the item in vData, update JSON, and refresh tree
            }
            hDragItem = nullptr;
            hDropTarget = nullptr;
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

// Add a new dialog procedure for the file view dialog (IDD_FILEVIEW)
INT_PTR CALLBACK fileViewDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hTree = nullptr;
    static HMENU hContextMenu = nullptr;
    static InsertionMark lastMark = {};
    switch (uMsg) {
    case WM_INITDIALOG: {
        stretch.setup(hwndDlg);
        hTree = GetDlgItem(hwndDlg, IDC_TREE1);
        // Create context menu
        hContextMenu = CreatePopupMenu();
        AppendMenu(hContextMenu, MF_STRING, ID_TREE_DELETE, L"Delete");
        HIMAGELIST hImages = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 2);
        HICON hIconFolder = LoadIcon(NULL, IDI_APPLICATION);
        HICON hIconFile = LoadIcon(NULL, IDI_APPLICATION);
        idxFolder = ImageList_AddIcon(hImages, hIconFolder);
        idxFile = ImageList_AddIcon(hImages, hIconFile);
        TreeView_SetImageList(hTree, hImages, TVSIL_NORMAL);

        // Increase item height and indent for better spacing
        // TODO: make this dynamic based on the font size of the UI
        TreeView_SetItemHeight(hTree, 20);  // Default is usually around 16-18 pixels
        TreeView_SetIndent(hTree, 18);      // Default is usually around 16-20 pixels

        // Set colors based on current theme
        updateTreeColorsExternal(hTree);

        lastMark = {};

        // Enable dark mode support
        npp(NPPM_DARKMODESUBCLASSANDTHEME, NPP::NppDarkMode::dmfInit, hwndDlg);

        return TRUE;
    }
    case WM_NOTIFY: {
        LPNMHDR nmhdr = (LPNMHDR)lParam;
        if (nmhdr->idFrom == IDC_TREE1) {
            LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
            switch (pnmtv->hdr.code) {
            case TVN_BEGINDRAG: {
                std::cout << "TVN_BEGINDRAG" << std::endl;
                hDragItem = pnmtv->itemNew.hItem;
                hDragImage = TreeView_CreateDragImage(hTree, hDragItem);
                if (hDragImage) {
                    POINT pt = pnmtv->ptDrag;
                    ClientToScreen(hTree, &pt);
                    ImageList_BeginDrag(hDragImage, 0, 0, 0);
                    ImageList_DragEnter(NULL, pt.x, pt.y);
                    SetCapture(hwndDlg);
                    isDragging = true;
                }
                return TRUE;
            }
            }
        }
        break;
    }
    case WM_CONTEXTMENU: {
        HWND hwndFrom = (HWND)wParam;
        if (hwndFrom == hTree) {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            // Select the item under the cursor
            TVHITTESTINFO hitTest = { 0 };
            hitTest.pt = pt;
            ScreenToClient(hTree, &hitTest.pt);
            HTREEITEM hItem = TreeView_HitTest(hTree, &hitTest);
            if (hItem) {
                TreeView_SelectItem(hTree, hItem);
            }
            // Show context menu
            TrackPopupMenu(hContextMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndDlg, NULL);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == ID_TREE_DELETE) {
            HTREEITEM hSel = TreeView_GetSelection(hTree);
            if (hSel) {
                // TODO: Remove from vData as well
                TreeView_DeleteItem(hTree, hSel);
            }
            return TRUE;
        }
        break;
    }
    case WM_MOUSEMOVE: {
        if (isDragging && hDragImage) {
            POINT pt;
            GetCursorPos(&pt); // screen coordinates
            ImageList_DragMove(pt.x, pt.y);

            // For hit-testing, convert to TreeView client coordinates
            POINT ptClient = pt;
            ScreenToClient(hTree, &ptClient);
            TVHITTESTINFO hitTest = { 0 };
            hitTest.pt = ptClient;
            HTREEITEM hItem = TreeView_HitTest(hTree, &hitTest);

            // Only show drop target if it's a valid target (not the dragged item)
            if (hItem && hItem != hDragItem) {
                TreeView_SelectDropTarget(hTree, hItem);
            }
            else {
                TreeView_SelectDropTarget(hTree, nullptr);
            }

            // --- Insertion line logic ---
            InsertionMark newMark = {};
            if (hItem && hItem != hDragItem) { // Don't show line over the dragged item itself
                RECT rc;
                TreeView_GetItemRect(hTree, hItem, &rc, TRUE);
                if (hitTest.flags & TVHT_ABOVE) {
                    newMark = { hItem, true, rc, true };
                }
                else if (hitTest.flags & TVHT_BELOW) {
                    newMark = { hItem, false, rc, true };
                }
                else {
                    // If on item, check if near top or bottom
                    int y = ptClient.y;
                    int mid = (rc.top + rc.bottom) / 2;
                    if (y < mid - 4) {
                        newMark = { hItem, true, rc, true };
                    }
                    else if (y > mid + 4) {
                        newMark = { hItem, false, rc, true };
                    }
                }
            }

            // Erase previous line if needed
            if (lastMark.valid && (!newMark.valid || lastMark.hItem != newMark.hItem || lastMark.above != newMark.above)) {
                std::string debugMsg = "Erasing last mark: " + std::to_string(lastMark.rect.top) + new_line;
                OutputDebugStringA(debugMsg.c_str());
                HDC hdc = GetDC(hTree);
                // Erase by drawing with background color
                BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);
                COLORREF bgColor = isDarkMode ? RGB(30, 30, 30) : RGB(255, 255, 255);
                HPEN hOldPen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 1, bgColor));
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, CreateSolidBrush(bgColor));
                Rectangle(hdc, lastMark.lineLeft, lastMark.lineY, lastMark.lineRight, lastMark.lineY + 4);
                SelectObject(hdc, hOldPen);
                SelectObject(hdc, hOldBrush);
                DeleteObject(GetCurrentObject(hdc, OBJ_PEN));
                DeleteObject(GetCurrentObject(hdc, OBJ_BRUSH));
                ReleaseDC(hTree, hdc);
                // Clear the entire lastMark structure, not just valid flag
                lastMark = {};
            }

            // Draw new line if needed
            if (newMark.valid && (!lastMark.valid || lastMark.hItem != newMark.hItem || lastMark.above != newMark.above)) {
                OutputDebugStringA("Draw new line\n");
                HDC hdc = GetDC(hTree);
                RECT rc = newMark.rect;
                int y = newMark.above ? rc.top : rc.bottom;
                // Calculate line coordinates
                int lineLeft = rc.left - 4;
                int lineRight = rc.right + 4;
                int lineY = y - 2;
                // Store coordinates for reliable erasing
                newMark.lineY = lineY;
                newMark.lineLeft = lineLeft;
                newMark.lineRight = lineRight;
                // Draw the line with theme-appropriate color
                BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);
                COLORREF lineColor = isDarkMode ? RGB(100, 150, 255) : RGB(0, 0, 255);  // Blue line
                HPEN hOldPen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 1, lineColor));
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, CreateSolidBrush(lineColor));
                Rectangle(hdc, lineLeft, lineY, lineRight, lineY + 4);
                SelectObject(hdc, hOldPen);
                SelectObject(hdc, hOldBrush);
                DeleteObject(GetCurrentObject(hdc, OBJ_PEN));
                DeleteObject(GetCurrentObject(hdc, OBJ_BRUSH));
                ReleaseDC(hTree, hdc);
                lastMark = newMark;
            }

            // Remove the redundant update - it's causing the issue
            // if (!newMark.valid && lastMark.valid) {
            //     lastMark = newMark; // This sets valid = false and clears other fields
            // }
            return TRUE;
        }
        else {
            // Not dragging - clear any lingering insertion line
            if (lastMark.valid) {
                OutputDebugStringA("Not dragging - clearing lingering line\n");
                HDC hdc = GetDC(hTree);
                // Erase by drawing with background color
                BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);
                COLORREF bgColor = isDarkMode ? RGB(30, 30, 30) : RGB(255, 255, 255);
                HPEN hOldPen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 1, bgColor));
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, CreateSolidBrush(bgColor));
                Rectangle(hdc, lastMark.lineLeft, lastMark.lineY, lastMark.lineRight, lastMark.lineY + 4);
                SelectObject(hdc, hOldPen);
                SelectObject(hdc, hOldBrush);
                DeleteObject(GetCurrentObject(hdc, OBJ_PEN));
                DeleteObject(GetCurrentObject(hdc, OBJ_BRUSH));
                ReleaseDC(hTree, hdc);
                lastMark = {};
            }
        }
        break;
    }
    case WM_LBUTTONUP: {
        if (isDragging) {
            // Erase insertion line if present
            if (lastMark.valid) {
                HDC hdc = GetDC(hTree);
                // Erase by drawing with background color
                BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);
                COLORREF bgColor = isDarkMode ? RGB(30, 30, 30) : RGB(255, 255, 255);
                HPEN hOldPen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 1, bgColor));
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, CreateSolidBrush(bgColor));
                Rectangle(hdc, lastMark.lineLeft, lastMark.lineY, lastMark.lineRight, lastMark.lineY + 4);
                SelectObject(hdc, hOldPen);
                SelectObject(hdc, hOldBrush);
                DeleteObject(GetCurrentObject(hdc, OBJ_PEN));
                DeleteObject(GetCurrentObject(hdc, OBJ_BRUSH));
                ReleaseDC(hTree, hdc);
                lastMark.valid = false;
            }
            ReleaseCapture();
            ImageList_EndDrag();
            ImageList_Destroy(hDragImage);
            hDragImage = nullptr;
            isDragging = false;

            // Clear drop target selection and restore normal selection
            TreeView_SelectDropTarget(hTree, nullptr);

            if (hDragItem && hDropTarget && hDragItem != hDropTarget) {
                // TODO: Move the item in vData, update JSON, and refresh tree
            }
            hDragItem = nullptr;
            hDropTarget = nullptr;
            return TRUE;
        }
        break;
    }
    case WM_CANCELMODE: { // In case drag is canceled
        if (lastMark.valid) {
            HDC hdc = GetDC(hTree);
            // Erase by drawing with background color
            BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);
            COLORREF bgColor = isDarkMode ? RGB(30, 30, 30) : RGB(255, 255, 255);
            HPEN hOldPen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 1, bgColor));
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, CreateSolidBrush(bgColor));
            Rectangle(hdc, lastMark.lineLeft, lastMark.lineY, lastMark.lineRight, lastMark.lineY + 4);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(GetCurrentObject(hdc, OBJ_PEN));
            DeleteObject(GetCurrentObject(hdc, OBJ_BRUSH));
            ReleaseDC(hTree, hdc);
            lastMark.valid = false;
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

void updateTreeColors(HWND hTree) {
    // Check if dark mode is enabled
    BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);
    
    if (isDarkMode) {
        // Dark mode colors
        TreeView_SetBkColor(hTree, RGB(30, 30, 30));  // Dark background
        TreeView_SetTextColor(hTree, RGB(220, 220, 220));  // Light text
    } else {
        // Light mode colors
        TreeView_SetBkColor(hTree, RGB(255, 255, 255));  // White background
        TreeView_SetTextColor(hTree, RGB(0, 0, 0));  // Black text
    }
}

}

void updateTreeColorsExternal(HWND hTree) {
    updateTreeColors(hTree);
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
        watcherPanel = CreateDialog(plugin.dllInstance, MAKEINTRESOURCE(IDD_FILEVIEW), plugin.nppData._nppHandle, fileViewDialogProc);
        NPP::tTbData dock;
        HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);

        
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
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    wcscpy_s(buffer, 100, toWchar(vFile.name));
    tvis.item.pszText = buffer;
    tvis.item.iImage = idxFile; // or idxFile
    tvis.item.iSelectedImage = idxFile; // or idxFile
    TreeView_InsertItem(hTree, &tvis);
}

void addFolderToTree(const VFolder vFolder, HWND hTree, HTREEITEM hParent)
{
    wchar_t buffer[100];

    TVINSERTSTRUCT tvis = { 0 };
    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    wcscpy_s(buffer, 100, toWchar(vFolder.name));
    tvis.item.pszText = buffer;
    tvis.item.iImage = idxFolder; // or idxFile
    tvis.item.iSelectedImage = idxFolder; // or idxFile
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