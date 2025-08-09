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
#include <functional>

#include <commctrl.h>
#include "model/VData.h"
#pragma comment(lib, "comctl32.lib")

#include <windowsx.h>
#include "ProcessCommands.h"

// External variables
extern CommonData commonData;




extern NPP::FuncItem menuDefinition[];  // Defined in Plugin.cpp
extern int menuItem_ToggleWatcher;      // Defined in Plugin.cpp

void writeJsonFile();
void syncVDataWithOpenFilesNotification();


wchar_t* toWchar(const std::string& str);
void addFileToTree(VFile* vFile, HWND hTree, HTREEITEM hParent);
void addFolderToTree(VFolder* vFolder, HWND hTree, HTREEITEM hParent, size_t& pos);
void resizeWatcherPanel();


#define ID_TREE_DELETE 40001
#define ID_FILE_CLOSE 40100
#define ID_FILE_WRAP_IN_FOLDER 40101



void updateTreeColorsExternal(HWND hTree);

HWND watcherPanel = 0;

namespace {

Scintilla::Position location = -1;
Scintilla::Position terminal = -1;

DialogStretch stretch;

std::wstring jsonFilePath;
bool contextMenuLoaded = false;

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

int getOrderFromTreeItem(HWND hTree, HTREEITEM hItem);
TVITEM getTreeItem(HWND hTree, HTREEITEM hItem);


// Helper function to find VFile by order
VFile* findVFileByOrder(VData& vData, int order) {
    // Search in root level files
    for (auto& file : vData.fileList) {
        if (file.order == order) {
            return &file;
        }
    }
    
    // If not found in root, search in folders
    for (auto& folder : vData.folderList) {
        for (auto& file : folder.fileList) {
            if (file.order == order) {
                return &file;
            }
        }
    }
    
    return nullptr;
}

// Helper functions for drag and drop reordering
int calculateNewOrder(int targetOrder, const InsertionMark& mark) {
    if (mark.above) {
        return targetOrder;  // Insert at target position
    } else {
        return targetOrder + 1;  // Insert after target
    }
}

void reorderItems(VData& vData, int oldOrder, int newOrder) {
    // Find the item to move
    VFile* movedFile = nullptr;
    for (auto& file : vData.fileList) {
        if (file.order == oldOrder) {
            movedFile = &file;
            break;
        }
    }
    
    if (!movedFile) return;
    
    // Adjust orders of other items
    if (oldOrder < newOrder) {
        // Moving down - decrease orders of items in between
        for (auto& file : vData.fileList) {
            if (file.order > oldOrder && file.order <= newOrder) {
                file.order--;
            }
        }
    } else {
        // Moving up - increase orders of items in between
        for (auto& file : vData.fileList) {
            if (file.order >= newOrder && file.order < oldOrder) {
                file.order++;
            }
        }
    }
    
    // Set new order
    movedFile->order = newOrder;
}

// Helper function to count total items (folders + files) recursively in a folder
int countItemsInFolder(const VFolder& folder) {
    int count = 1; // Count the folder itself
    
    // Count all files in this folder
    count += folder.fileList.size();
    
    // Recursively count items in subfolders
    for (const auto& subFolder : folder.folderList) {
        count += countItemsInFolder(subFolder);
    }
    
    return count;
}

// Helper function to adjust global orders when moving a folder with all its contents
void adjustGlobalOrdersForFolderMove(VData& vData, int oldOrder, int newOrder, int folderItemCount) {
    // Get all files across the entire hierarchy
    vector<VFile*> allFiles = vData.getAllFiles();
    
    // Helper lambda to get all folders recursively
    std::function<void(vector<VFolder>&, vector<VFolder*>&)> getAllFolders = 
        [&](vector<VFolder>& folders, vector<VFolder*>& result) {
            for (auto& folder : folders) {
                result.push_back(&folder);
                getAllFolders(folder.folderList, result);
            }
        };
    
    vector<VFolder*> allFolders;
    getAllFolders(vData.folderList, allFolders);
    
    if (oldOrder < newOrder) {
        // Moving down - shift items in between to the left
        int posShiftAmount = newOrder - folderItemCount;
		int negShiftAmount = folderItemCount;
		int startOrder = oldOrder + folderItemCount; // Start from the next item

        
        // Adjust files
        for (auto* file : allFiles) {
            if (file->order >= startOrder && file->order <= newOrder) {
                file->order -= negShiftAmount;
            }
        }
        
        // Adjust folders
        for (auto* folder : allFolders) {
            if (folder->order >= startOrder && folder->order <= newOrder) {
                folder->order -= negShiftAmount;
            }
        }
    } else {
        // Moving up - shift items in between to the right
        int shiftAmount = folderItemCount;
        
        // Adjust files
        for (auto* file : allFiles) {
            if (file->order >= newOrder && file->order < oldOrder) {
                file->order += shiftAmount;
            }
        }
        
        // Adjust folders
        for (auto* folder : allFolders) {
            if (folder->order >= newOrder && folder->order < oldOrder) {
                folder->order += shiftAmount;
            }
        }
    }
}

// Helper function to recursively adjust orders within a container
void adjustOrdersInContainer(vector<VFolder>& folders, vector<VFile>& files, int oldOrder, int newOrder) {
    if (oldOrder < newOrder) {
        // Moving down - decrease orders of items in between
        for (auto& folder : folders) {
            if (folder.order > oldOrder && folder.order <= newOrder) {
                folder.order--;
            }
        }
        for (auto& file : files) {
            if (file.order > oldOrder && file.order <= newOrder) {
                file.order--;
            }
        }
    } else {
        // Moving up - increase orders of items in between
        for (auto& folder : folders) {
            if (folder.order >= newOrder && folder.order < oldOrder) {
                folder.order++;
            }
        }
        for (auto& file : files) {
            if (file.order >= newOrder && file.order < oldOrder) {
                file.order++;
            }
        }
    }
}

// Helper function to find which container holds a folder with the given order
struct FolderLocation {
    VFolder* parentFolder = nullptr;  // nullptr means root level
    VFolder* folder = nullptr;
    bool found = false;
};

FolderLocation findFolderLocation(VData& vData, int order) {
    FolderLocation location;
    
    // Check root level first
    for (auto& folder : vData.folderList) {
        if (folder.order == order) {
            location.folder = &folder;
            location.parentFolder = nullptr;  // Root level
            location.found = true;
            return location;
        }
    }
    
    // Recursively search in subfolders
    std::function<bool(VFolder&)> searchInFolder = [&](VFolder& parent) -> bool {
        for (auto& folder : parent.folderList) {
            if (folder.order == order) {
                location.folder = &folder;
                location.parentFolder = &parent;
                location.found = true;
                return true;
            }
            // Recursive search in this folder's subfolders
            if (searchInFolder(folder)) {
                return true;
            }
        }
        return false;
    };
    
    // Search in all root-level folders
    for (auto& rootFolder : vData.folderList) {
        if (searchInFolder(rootFolder)) {
            break;
        }
    }
    
    return location;
}

void reorderFolders(VData& vData, int oldOrder, int newOrder) {
    // Find the folder to move using recursive search
    FolderLocation moveLocation = findFolderLocation(vData, oldOrder);
    
    if (!moveLocation.found) {
        return;  // Folder not found
    }
    
    VFolder* movedFolder = moveLocation.folder;
    
    // Count total items in the folder (folder itself + all contents recursively)
    int folderItemCount = countItemsInFolder(*movedFolder);
    newOrder--;
    
    // Adjust global orders to make space for the moved folder and its contents
    adjustGlobalOrdersForFolderMove(vData, oldOrder, newOrder, folderItemCount);
    
    // Set the new order for the moved folder
    //movedFolder->order = newOrder;
    movedFolder->move((newOrder - oldOrder) - (folderItemCount - 1));
    
    // After reordering, recursively sort the entire data structure to ensure consistency
    vData.vDataSort();
}

void refreshTree(HWND hTree, VData& vData) {
    // Clear existing tree
    TreeView_DeleteAllItems(hTree);
    
    // Rebuild tree with new order  
	vData.vDataSort();
    
    int lastOrder = vData.folderList.empty() ? 0 : vData.folderList.back().order;
    lastOrder = std::max(lastOrder, vData.fileList.empty() ? 0 : vData.fileList.back().order);
    for (size_t pos = 0; pos <= lastOrder; pos) {
        optional<VFile*> vFile = vData.findFileByOrder(pos);
        if (!vFile) {
            optional<VFolder*> vFolder = vData.findFolderByOrder(pos);
            if (vFolder) {
                addFolderToTree(vFolder.value(), hTree, TVI_ROOT, pos);
            }
        } else {
            addFileToTree(vFile.value(), hTree, TVI_ROOT);
            pos++;
        }
    }
}

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


// Add a new dialog procedure for the file view dialog (IDD_FILEVIEW)
INT_PTR CALLBACK fileViewDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hTree = nullptr;
    static HMENU hContextMenu = nullptr;
    static HMENU fileContextMenu = nullptr;
    static InsertionMark lastMark = {};
    switch (uMsg) {
    case WM_INITDIALOG: {
        stretch.setup(hwndDlg);
        hTree = GetDlgItem(hwndDlg, IDC_TREE1);
        // Create context menu
        hContextMenu = CreatePopupMenu();
        AppendMenu(hContextMenu, MF_STRING, ID_TREE_DELETE, L"Delete");

        fileContextMenu = CreatePopupMenu();
        AppendMenu(fileContextMenu, MF_STRING, ID_FILE_CLOSE, L"Close");
        AppendMenu(fileContextMenu, MF_STRING, ID_FILE_WRAP_IN_FOLDER, L"Wrap in folder");


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
            case TVN_SELCHANGED: {
                // Only process file clicks if Notepad++ is ready
                if (!commonData.isNppReady) {
                    return TRUE;  // Skip processing if Notepad++ isn't ready yet
                }

                // Handle file selection - open file when user clicks on a file item
                HTREEITEM hSelectedItem = pnmtv->itemNew.hItem;
                if (hSelectedItem) {
                    TVITEM item = { 0 };
                    item.mask = TVIF_IMAGE | TVIF_PARAM;
                    item.hItem = hSelectedItem;
                    if (TreeView_GetItem(hTree, &item)) {
                        // Check if this is a file (not a folder) by checking the image index
                        if (item.iImage != idxFile) {
							return TRUE;  // This is a file item, no need to do anything here
                        }
                            
                        // This is a file item, get the order and find the corresponding VFile
                        int order = static_cast<int>(item.lParam);
                            
                        // Find the VFile using the helper function
                        optional<VFile*> selectedFile = commonData.vData.findFileByOrder(order);
                            
                        // If file found and has a valid path, open it
                        if (selectedFile && !selectedFile.value()->path.empty()) {
                            // Convert path to wide string
                            std::wstring widePath(selectedFile.value()->path.begin(), selectedFile.value()->path.end());
                                
                            // Debug output
                            OutputDebugStringA(("Opening file: " + selectedFile.value()->path).c_str());
                                
                            // First, try to switch to the file if it's already open
                            if (!npp(NPPM_SWITCHTOFILE, 0, reinterpret_cast<LPARAM>(widePath.c_str()))) {
                                // If file is not open, open it
                                npp(NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(widePath.c_str()));
                            }
                                
                            // Optionally, switch to the appropriate view if specified
                            if (selectedFile.value()->view >= 0) {
                                // Switch to the specified view (0 = main view, 1 = sub view)
                                npp(NPPM_ACTIVATEDOC, selectedFile.value()->view, selectedFile.value()->docOrder);
                            }
                        }
                        else if (selectedFile && selectedFile.value()->path.empty()) {
                            OutputDebugStringA("File has empty path, cannot open");
                        }
                        else {
                            OutputDebugStringA("File not found in vData");
                        }
                    }
                }
                return TRUE;
            }
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
                    
                    // Start tracking mouse leave events
                    TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hTree, 0 };
                    TrackMouseEvent(&tme);
                }
                return TRUE;
            }
            case NM_DBLCLK: {
                // Handle double-click on tree items
                LPNMITEMACTIVATE pnmia = (LPNMITEMACTIVATE)lParam;
                if (pnmia->iItem != -1) {
                    // Get the selected item from the tree view
                    HTREEITEM hSelectedItem = TreeView_GetSelection(hTree);
                    if (hSelectedItem) {
                        TVITEM item = { 0 };
                        item.mask = TVIF_IMAGE | TVIF_PARAM;
                        item.hItem = hSelectedItem;
                        if (TreeView_GetItem(hTree, &item)) {
                            // Check if this is a file (not a folder) by checking the image index
                            if (item.iImage == idxFile) {
                                // This is a file item, get the order and find the corresponding VFile
                                int order = static_cast<int>(item.lParam);
                                
                                // Find the VFile using the helper function
                                VFile* selectedFile = findVFileByOrder(commonData.vData, order);
                                
                                // If file found and has a valid path, open it
                                if (selectedFile && !selectedFile->path.empty()) {
                                    // Convert path to wide string
                                    std::wstring widePath(selectedFile->path.begin(), selectedFile->path.end());
                                    
                                    // Debug output
                                    OutputDebugStringA(("Double-click opening file: " + selectedFile->path).c_str());
                                    
                                    // First, try to switch to the file if it's already open
                                    if (!npp(NPPM_SWITCHTOFILE, 0, reinterpret_cast<LPARAM>(widePath.c_str()))) {
                                        // If file is not open, open it
                                        npp(NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(widePath.c_str()));
                                    }
                                    
                                    // Optionally, switch to the appropriate view if specified
                                    if (selectedFile->view >= 0) {
                                        // Switch to the specified view (0 = main view, 1 = sub view)
                                        npp(NPPM_ACTIVATEDOC, selectedFile->view, 0);
                                    }
                                }
                            }
                        }
                    }
                }
                return TRUE;
            }
            case NM_RCLICK: {
                DWORD pos = GetMessagePos();
                POINT pt = { GET_X_LPARAM(pos), GET_Y_LPARAM(pos) };

                TVHITTESTINFO hit = {0};
                hit.pt = pt;
                ScreenToClient(hTree, &hit.pt);
                HTREEITEM hItem = TreeView_HitTest(hTree, &hit);

                TVITEM tvItem = getTreeItem(hTree, hItem);

                if (hItem && (hit.flags & TVHT_ONITEM))
                {
                    TreeView_SelectItem(hTree, hItem);
                    // ShowContextMenu(hWndParent, pt, hItem);
                    TrackPopupMenu(fileContextMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndDlg, NULL);
					contextMenuLoaded = true;
                }
                return TRUE;
            }
            }
        }
        break;
    }
    case WM_CONTEXTMENU: {
        if (contextMenuLoaded) {
            contextMenuLoaded = false; // Reset flag
            return TRUE; // Prevent default context menu handling
		}
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

            TVITEM item = { 0 };
            item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_PARAM;
            item.hItem = hItem;
            if (TreeView_GetItem(hTree, &item)) {
                // Check if this is a file (not a folder) by checking the image index
                if (item.iImage == idxFile) {
                    return TRUE;  // This is a file item, no need to do anything here
                }
            }

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
                //TreeView_DeleteItem(hTree, hSel);
            }
            return TRUE;
        }
        else if (LOWORD(wParam) == ID_FILE_CLOSE) {
            HTREEITEM hSel = TreeView_GetSelection(hTree);
            if (hSel) {
                TreeView_DeleteItem(hTree, hSel);
                // TODO: Remove from vData as well
				TVITEM item = getTreeItem(hTree, hSel);

            }
            return TRUE;
        }
        break;
    }
    case WM_SIZE: {
        // When the dialog is resized, resize the tree control to fill it
        if (hTree) {
            RECT rcClient;
            GetClientRect(hwndDlg, &rcClient);
            SetWindowPos(hTree, nullptr, 0, 0, 
                        rcClient.right - rcClient.left, 
                        rcClient.bottom - rcClient.top, 
                        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            
            // Force the tree control to update its scrollbars
            InvalidateRect(hTree, nullptr, TRUE);
            UpdateWindow(hTree);
        }
        return TRUE;
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

            // Check if mouse is outside the tree area
            RECT treeRect;
            GetClientRect(hTree, &treeRect);
            bool mouseOutsideTree = (ptClient.x < 0 || ptClient.x >= treeRect.right || 
                                   ptClient.y < 0 || ptClient.y >= treeRect.bottom);

            // Only show drop target if it's a valid target (not the dragged item)
            if (hItem && hItem != hDragItem) {
                TreeView_SelectDropTarget(hTree, hItem);
                hDropTarget = hItem;
            }
            else {
                TreeView_SelectDropTarget(hTree, nullptr);
                hDropTarget = nullptr;
            }

            // --- Insertion line logic ---
            InsertionMark newMark = {};
            if (hItem && hItem != hDragItem && !mouseOutsideTree) { // Don't show line over the dragged item itself or outside tree
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
                std::string debugMsg = "Erasing last mark: left=" + std::to_string(lastMark.lineLeft) + 
                                     ", right=" + std::to_string(lastMark.lineRight) + 
                                     ", y=" + std::to_string(lastMark.lineY) + new_line;
                OutputDebugStringA(debugMsg.c_str());
                HDC hdc = GetDC(hTree);
                
                // Get the actual background color from the tree control
                COLORREF bgColor = TreeView_GetBkColor(hTree);
                
                HPEN hOldPen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 1, bgColor));
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, CreateSolidBrush(bgColor));
                
                // Erase a slightly larger area to ensure complete coverage
                Rectangle(hdc, lastMark.lineLeft - 2, lastMark.lineY - 1, lastMark.lineRight, lastMark.lineY + 5);
                SelectObject(hdc, hOldPen);
                SelectObject(hdc, hOldBrush);
                DeleteObject(GetCurrentObject(hdc, OBJ_PEN));
                DeleteObject(GetCurrentObject(hdc, OBJ_BRUSH));
                ReleaseDC(hTree, hdc);
                
                // Force redraw of a larger area to ensure erasure is visible
                RECT eraseRect = { lastMark.lineLeft - 2, lastMark.lineY - 1, lastMark.lineRight, lastMark.lineY + 5 };
                InvalidateRect(hTree, &eraseRect, FALSE);
                UpdateWindow(hTree);
                
                // Clear the entire lastMark structure, not just valid flag
                lastMark = {};
            }

            // Draw new line if needed
            if (newMark.valid && (!lastMark.valid || lastMark.hItem != newMark.hItem || lastMark.above != newMark.above)) {
                OutputDebugStringA("Draw new line\n");
                HDC hdc = GetDC(hTree);
                RECT rc = newMark.rect;
                int y = newMark.above ? rc.top : rc.bottom;
                
                // Calculate line coordinates - use item bounds with small extension
                int lineLeft = rc.left - 4;
                int lineRight = rc.right + 4;
                int lineY = y - 2;
                
                // Store coordinates for reliable erasing
                newMark.lineY = lineY;
                newMark.lineLeft = lineLeft;
                newMark.lineRight = lineRight;
                
                // Draw the line with theme-appropriate color
                BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);
                COLORREF lineColor = isDarkMode ? RGB(255, 255, 255) : RGB(0, 0, 255);  // White line in dark mode, blue in light mode
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
                
                // Get the actual background color from the tree control
                COLORREF bgColor = TreeView_GetBkColor(hTree);
                
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
    case WM_MOUSELEAVE: {
        // Clear insertion line when mouse leaves the tree area during dragging
        if (isDragging && lastMark.valid) {
            OutputDebugStringA("Mouse left tree area during drag - clearing line\n");
            HDC hdc = GetDC(hTree);
            
            // Get the actual background color from the tree control
            COLORREF bgColor = TreeView_GetBkColor(hTree);
            
            HPEN hOldPen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 1, bgColor));
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, CreateSolidBrush(bgColor));
            
            // Erase a slightly larger area to ensure complete coverage
            Rectangle(hdc, lastMark.lineLeft - 2, lastMark.lineY - 1, lastMark.lineRight + 2, lastMark.lineY + 5);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(GetCurrentObject(hdc, OBJ_PEN));
            DeleteObject(GetCurrentObject(hdc, OBJ_BRUSH));
            ReleaseDC(hTree, hdc);
            
            // Force redraw of a larger area to ensure erasure is visible
            RECT eraseRect = { lastMark.lineLeft - 2, lastMark.lineY - 1, lastMark.lineRight + 2, lastMark.lineY + 5 };
            InvalidateRect(hTree, &eraseRect, FALSE);
            UpdateWindow(hTree);
            
            lastMark = {};
        }
        break;
    }
    case WM_LBUTTONUP: {
        bool insertion = lastMark.valid;
        if (isDragging) {
            // Erase insertion line if present
            if (lastMark.valid) {
                HDC hdc = GetDC(hTree);
                
                // Get the actual background color from the tree control
                COLORREF bgColor = TreeView_GetBkColor(hTree);
                
                HPEN hOldPen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 1, bgColor));
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, CreateSolidBrush(bgColor));
                
                // Erase a slightly larger area to ensure complete coverage
                Rectangle(hdc, lastMark.lineLeft - 2, lastMark.lineY - 1, lastMark.lineRight + 2, lastMark.lineY + 5);
                SelectObject(hdc, hOldPen);
                SelectObject(hdc, hOldBrush);
                DeleteObject(GetCurrentObject(hdc, OBJ_PEN));
                DeleteObject(GetCurrentObject(hdc, OBJ_BRUSH));
                ReleaseDC(hTree, hdc);
                
                // Force redraw of a larger area to ensure erasure is visible
                RECT eraseRect = { lastMark.lineLeft - 2, lastMark.lineY - 1, lastMark.lineRight + 2, lastMark.lineY + 5 };
                InvalidateRect(hTree, &eraseRect, FALSE);
                UpdateWindow(hTree);
                
                lastMark.valid = false;
            }
            ReleaseCapture();
            ImageList_EndDrag();
            ImageList_Destroy(hDragImage);
            hDragImage = nullptr;
            isDragging = false;

            // Clear drop target selection and restore normal selection
            TreeView_SelectDropTarget(hTree, nullptr);

            if (hDragItem && hDropTarget && hDragItem != hDropTarget && insertion) {
                // Get the dragged and target item orders
                int dragOrder = getOrderFromTreeItem(hTree, hDragItem);
				int targetOrder = getOrderFromTreeItem(hTree, hDropTarget);

                // Find the dragged item in vData
                optional<VFile*> draggedFileOpt = commonData.vData.findFileByOrder(dragOrder);
                optional<VFolder*> draggedFolderOpt = commonData.vData.findFolderByOrder(dragOrder);
                
                if (draggedFileOpt) {
                    // Calculate new order based on drop position
                    int newOrder = calculateNewOrder(targetOrder, lastMark);
                    
                    // Update the VFile order
                    draggedFileOpt.value()->order = newOrder;
                    
                    // Reorder other items
                    reorderItems(commonData.vData, dragOrder, newOrder);
                    
                    // Refresh the tree
                    refreshTree(hTree, commonData.vData);
                    
                    // Save to JSON
                    writeJsonFile();
                }
                else if (draggedFolderOpt) {
                    // Similar logic for folders
                    int newOrder = calculateNewOrder(targetOrder, lastMark);
                    //draggedFolderOpt.value()->order = newOrder;
                    reorderFolders(commonData.vData, dragOrder, newOrder);
                    refreshTree(hTree, commonData.vData);
                    writeJsonFile();
                }
            }
            hDragItem = nullptr;
            hDropTarget = nullptr;
            return TRUE;
        }
        break;
    }
    case WM_CANCELMODE: { // In case drag is canceled
        if (lastMark.valid) {
            OutputDebugStringA("WM_CANCELMODE\n");
            HDC hdc = GetDC(hTree);
            
            // Get the actual background color from the tree control
            COLORREF bgColor = TreeView_GetBkColor(hTree);
            
            HPEN hOldPen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 1, bgColor));
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, CreateSolidBrush(bgColor));
            
            // Erase a slightly larger area to ensure complete coverage
            Rectangle(hdc, lastMark.lineLeft - 2, lastMark.lineY - 1, lastMark.lineRight + 2, lastMark.lineY + 5);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(GetCurrentObject(hdc, OBJ_PEN));
            DeleteObject(GetCurrentObject(hdc, OBJ_BRUSH));
            ReleaseDC(hTree, hdc);
            
            // Force redraw of a larger area to ensure erasure is visible
            RECT eraseRect = { lastMark.lineLeft - 2, lastMark.lineY - 1, lastMark.lineRight + 2, lastMark.lineY + 5 };
            InvalidateRect(hTree, &eraseRect, FALSE);
            UpdateWindow(hTree);
            
            lastMark.valid = false;
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

int getOrderFromTreeItem(HWND hTree, HTREEITEM hItem) {
    TVITEM tvi = { 0 };
    tvi.mask = TVIF_TEXT | TVIF_PARAM;   // We only care about lParam
    tvi.hItem = hItem;       // The HTREEITEM you have

    if (TreeView_GetItem(hTree, &tvi)) {
        int storedNumber = (int)tvi.lParam;
		return storedNumber; // lParam contains the order
    }

    return -1; // Not found or error
}

TVITEM getTreeItem(HWND hTree, HTREEITEM hItem) {
    TVITEM tvi = { 0 };
    tvi.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM; // Include all necessary flags
    tvi.hItem = hItem;
    TCHAR text[256] = { 0 };
    tvi.pszText = text;
    tvi.cchTextMax = sizeof(text) / sizeof(TCHAR);
    
    if (TreeView_GetItem(hTree, &tvi)) {
        return tvi; // Return the filled TVITEM structure
    }
    
    return {}; // Return an empty structure if not found
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

void resizeWatcherPanel() {
    if (!watcherPanel || !IsWindow(watcherPanel)) return;
    
    // Get the current dialog position and size
    RECT rcDialog;
    GetWindowRect(watcherPanel, &rcDialog);
    
    // Get Notepad++ window dimensions
    RECT rcNpp;
    GetWindowRect(plugin.nppData._nppHandle, &rcNpp);
    
    // Calculate the maximum available width for the left panel
    // Use a larger portion of the window width for better usability
    int nppWidth = rcNpp.right - rcNpp.left;
    int nppHeight = rcNpp.bottom - rcNpp.top;
    int maxPanelWidth = nppWidth * 2 / 3;  // Allow up to 2/3 of the window width
    int panelWidth = maxPanelWidth;
    int panelHeight = nppHeight - 100; // Leave some space for menu/toolbar
    
    // Set minimum sizes to ensure usability
    const int MIN_WIDTH = 200;
    const int MIN_HEIGHT = 150;
    
    panelWidth = max(panelWidth, MIN_WIDTH);
    panelHeight = max(panelHeight, MIN_HEIGHT);
    
    // Resize the dialog to fill the available space
    SetWindowPos(watcherPanel, nullptr, 0, 0, panelWidth, panelHeight, 
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    
    // Resize the tree control to fill the dialog
    HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);
    if (hTree) {
        RECT rcClient;
        GetClientRect(watcherPanel, &rcClient);
        SetWindowPos(hTree, nullptr, 0, 0, 
                    rcClient.right - rcClient.left, 
                    rcClient.bottom - rcClient.top, 
                    SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        
        // Force the tree control to update its scrollbars
        InvalidateRect(hTree, nullptr, TRUE);
        UpdateWindow(hTree);
    }
}

void updateWatcherPanel() { if (watcherPanel && IsWindowVisible(watcherPanel)) updateWatcherPanelUnconditional(); }



void toggleWatcherPanelWithList() {
    if (!watcherPanel) {
        watcherPanel = CreateDialog(plugin.dllInstance, MAKEINTRESOURCE(IDD_FILEVIEW), plugin.nppData._nppHandle, fileViewDialogProc);
        NPP::tTbData dock;
        HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);

        // Resize dialog to match left panel size
        resizeWatcherPanel();

        
        TCHAR configDir[MAX_PATH];
        ::SendMessage(plugin.nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)configDir);
        jsonFilePath = std::wstring(configDir) + L"\\virtualfolders.json";

        // read JSON
        json vDataJson = loadVDataFromFile(jsonFilePath);
        commonData.vData = vDataJson.get<VData>();

        commonData.openFiles = listOpenFiles();
        
        // Sync vData with open files
        syncVDataWithOpenFiles(commonData.vData, commonData.openFiles);
        

		
        writeJsonFile();
        

        commonData.vData.vDataSort();
		
        int lastOrder = commonData.vData.folderList.empty() ? 0 : commonData.vData.folderList.back().order;
		lastOrder = std::max(lastOrder, commonData.vData.fileList.empty() ? 0 : commonData.vData.fileList.back().order);

        for (size_t pos = 0; pos <= lastOrder; pos) {
			auto vFile = commonData.vData.findFileByOrder(pos);
            if (!vFile) {
                auto vFolder = commonData.vData.findFolderByOrder(pos);
                if (vFolder) {
                    addFolderToTree(vFolder.value(), hTree, TVI_ROOT, pos);
                }
            }
            else {
                addFileToTree(vFile.value(), hTree, TVI_ROOT);
                pos++;
            }

        }
        

        dock.hClient = watcherPanel;
        dock.pszName = L"Virtual Folders";  // title bar text (caption in dialog is replaced)
        dock.dlgID = menuItem_ToggleWatcher;          // zero-based position in menu to recall dialog at next startup
        dock.uMask = DWS_DF_CONT_LEFT | DWS_ICONTAB;
        dock.pszModuleName = L"VFolders.dll";        // plugin module name
        HICON hIcon = LoadIcon(plugin.dllInstance, MAKEINTRESOURCE(IDI_FOLDER_YELLOW));
        dock.hIconTab = hIcon;


        npp(NPPM_DMMREGASDCKDLG, 0, &dock);

        OutputDebugStringA("Watch Panel Create\n");
    }
    else if (IsWindowVisible(watcherPanel)) {
        npp(NPPM_DMMHIDE, 0, watcherPanel);
        OutputDebugStringA("Watch Panel Hide\n");
        // Update tab selection state
        commonData.virtualFoldersTabSelected = false;
    }
    else {
        updateWatcherPanelUnconditional();
        npp(NPPM_DMMSHOW, 0, watcherPanel);
        OutputDebugStringA("Watch Panel Show\n");
        // Update tab selection state
        commonData.virtualFoldersTabSelected = true;
    }
}

void writeJsonFile() {
    json vDataJson = commonData.vData;
    std::ofstream(jsonFilePath) << vDataJson.dump(4); // Write JSON to file
}

void syncVDataWithOpenFilesNotification() {
    // Get current open files
    std::vector<VFile> openFiles = listOpenFiles();
    
    // Sync vData with current open files
    syncVDataWithOpenFiles(commonData.vData, openFiles);
    
    // Update the UI
    updateWatcherPanel();
    
    // Optionally save to JSON
    // writeJsonFile(); // Uncomment if you want to save immediately
}

wchar_t* toWchar(const std::string& str) {
	static wchar_t buffer[256];
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer, 256);
	return buffer;
}

void addFileToTree(VFile* vFile, HWND hTree, HTREEITEM hParent)
{
    wchar_t buffer[100];

    TVINSERTSTRUCT tvis = { 0 };
    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
    wcscpy_s(buffer, 100, toWchar(vFile->name));
    tvis.item.pszText = buffer;
    tvis.item.iImage = idxFile; // or idxFile
    tvis.item.iSelectedImage = idxFile; // or idxFile
	tvis.item.lParam = vFile->order; // Store the order/index directly in lparam

    //tvis.item.lParam = reinterpret_cast<LPARAM>(&vFile);

    HTREEITEM hItem = TreeView_InsertItem(hTree, &tvis);
    

    //VFile vFile2 = *reinterpret_cast<VFile*>(tvis.item.lParam);


    if (vFile->isActive) {
        TreeView_SelectItem(hTree, hItem);
        TreeView_EnsureVisible(hTree, hItem);
    }
}

void addFolderToTree(VFolder* vFolder, HWND hTree, HTREEITEM hParent, size_t& pos)
{
    wchar_t buffer[100];

    TVINSERTSTRUCT tvis = { 0 };
    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
    wcscpy_s(buffer, 100, toWchar(vFolder->name));
    tvis.item.pszText = buffer;
    tvis.item.iImage = idxFolder; // or idxFile
    tvis.item.iSelectedImage = idxFolder; // or idxFile
	tvis.item.lParam = vFolder->order; // Store the order/index directly in lparam
    HTREEITEM hFolder = TreeView_InsertItem(hTree, &tvis);


    //TVITEM tvItem = getTreeItem(hTree, hFolder);
    
	pos++;
	
    int lastOrder = vFolder->fileList.empty() ? 0 : vFolder->fileList.back().order;
	lastOrder = std::max(lastOrder, vFolder->folderList.empty() ? 0 : vFolder->folderList.back().order);
    for (pos; pos <= lastOrder; pos) {
        optional<VFile*> vFile = vFolder->findFileByOrder(pos);
        if (vFile) {
            addFileToTree(*vFile, hTree, hFolder);
            pos++;
        }
        else {
			auto vSubFolder = vFolder->findFolderByOrder(pos);
			if (vSubFolder) {
				addFolderToTree(*vSubFolder, hTree, hFolder, pos);
			}
        }
    }


    if (vFolder->isExpanded) {
        TreeView_Expand(hTree, hFolder, TVE_EXPAND);
    }
}