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

#include "TreeViewManager.h"
#include "CommonData.h"
#include "resource.h"
#include "Shlwapi.h"
#include "RenameDialog.h"

#include <fstream>
#include <iostream>
#include <functional>
#include <algorithm>
#define NOMINMAX
#include <windowsx.h>

#pragma comment(lib, "comctl32.lib")

// External variables
extern CommonData commonData;
extern NPP::FuncItem menuDefinition[];
extern int menuItem_ToggleWatcher;


std::unordered_map<IconType, int> iconIndex;
std::string new_line = "\n";

DialogStretch stretch;







// Forward declarations
void writeJsonFile();
void updateWatcherPanelUnconditional(UINT_PTR bufferID);

void treeItemSelected(HTREEITEM selectedTreeItem);
void moveFileIntoFolder(int dragOrder, int targetOrder);
void moveFolderIntoFolder(int dragOrder, int targetOrder);
void unwrapFolder();
void wrapFileInFolder();


// External variables
extern HWND watcherPanel;



// TreeView management functions
void updateTreeItemLParam(VBase* vBase) {
    if (vBase->hTreeItem) {
        TVITEM tvi = { 0 };
        tvi.mask = TVIF_PARAM;
        tvi.hItem = (vBase->hTreeItem);
        tvi.lParam = vBase->order;
        TreeView_SetItem(commonData.hTree, &tvi);
    }
}

int calculateNewOrder(int targetOrder, const InsertionMark& mark) {
    if (mark.above) {
        return targetOrder;  // Insert at target position
    }
    else {
        return targetOrder + 1;  // Insert after target
    }
}

// Add a new dialog procedure for the file view dialog (IDD_FILEVIEW)
INT_PTR CALLBACK fileViewDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hTree = nullptr;
    static HMENU hContextMenu = nullptr;
    static HMENU fileContextMenu = nullptr;
	static HMENU folderContextMenu = nullptr;
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
        AppendMenu(fileContextMenu, MF_STRING, ID_FILE_RENAME, L"Rename");

		folderContextMenu = CreatePopupMenu();
		AppendMenu(folderContextMenu, MF_STRING, ID_FOLDER_UNWRAP, L"Unwrap");
		AppendMenu(folderContextMenu, MF_STRING, ID_FOLDER_RENAME, L"Rename");


        HIMAGELIST hImages = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 2);
        //HICON hIconFolder = LoadIcon(NULL, IDI_APPLICATION);
        HICON hIconFolder = LoadIcon(plugin.dllInstance, MAKEINTRESOURCE(IDI_FOLDER_YELLOW));
        HICON hIconFile = LoadIcon(NULL, IDI_APPLICATION);
        HICON hIconFileLight = LoadIcon(plugin.dllInstance, MAKEINTRESOURCE(IDI_FILE_LIGHT_ICON));
        HICON hIconFileDark = LoadIcon(plugin.dllInstance, MAKEINTRESOURCE(IDI_FILE_DARK_ICON));
        HICON hIconFileEdited = LoadIcon(plugin.dllInstance, MAKEINTRESOURCE(IDI_FILE_EDITED_ICON));

        

        iconIndex[ICON_FOLDER] = ImageList_AddIcon(hImages, hIconFolder);
        iconIndex[ICON_FILE] = ImageList_AddIcon(hImages, hIconFile);
        iconIndex[ICON_FILE_LIGHT] = ImageList_AddIcon(hImages, hIconFileLight);
        iconIndex[ICON_FILE_DARK] = ImageList_AddIcon(hImages, hIconFileDark);
        iconIndex[ICON_FILE_EDITED] = ImageList_AddIcon(hImages, hIconFileEdited);

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
                } else if (ignoreSelectionChange) {
                    ignoreSelectionChange = false;  // Reset the flag
                    //return TRUE;  // Skip processing if we are ignoring this change
				}
				HTREEITEM selectedTreeItem = pnmtv->itemNew.hItem;
				treeItemSelected(selectedTreeItem);
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

                    TVITEM item = { 0 };
                    item.mask = TVIF_IMAGE | TVIF_PARAM;
                    item.hItem = hItem;
                    if (TreeView_GetItem(hTree, &item)) {
                        // Check if this is a folder (not a file) by checking the image index
                        if (item.iImage == iconIndex[ICON_FOLDER]) {
                            TrackPopupMenu(folderContextMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndDlg, NULL);
                        }
                        else {
                            TrackPopupMenu(fileContextMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndDlg, NULL);
                        }
					    contextMenuLoaded = true;
                    }

                }
                return TRUE;
            }
            }
        }
        if (nmhdr->code == TVN_ITEMEXPANDED) {
			if (!commonData.isNppReady) return TRUE;  // Skip processing if Notepad++ isn't ready yet

            LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
            HTREEITEM hItem = pnmtv->itemNew.hItem;
            TVITEM item = { 0 };
            item.mask = TVIF_PARAM;
            item.hItem = hItem;
            if (TreeView_GetItem(hTree, &item)) {
                optional<VFolder*> vFolderOpt = commonData.vData.findFolderByOrder((int)item.lParam);
                if (vFolderOpt) {
                    vFolderOpt.value()->isExpanded = pnmtv->action == TVE_EXPAND;
                }
            }
            return TRUE;
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
                if (item.iImage != iconIndex[ICON_FOLDER]) {
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
				TVITEM item = getTreeItem(hTree, hSel);
				optional<VFile*> vFileOpt = commonData.vData.findFileByOrder((int)item.lParam);
                if (vFileOpt) {
                    UINT_PTR bufferID = npp(NPPM_GETBUFFERIDFROMPOS, vFileOpt.value()->docOrder, vFileOpt.value()->view);
                    npp(NPPM_MENUCOMMAND, bufferID, IDM_FILE_CLOSE);
                }
            }
            return TRUE;
        } 
        else if (LOWORD(wParam) == ID_FILE_WRAP_IN_FOLDER) {
            wrapFileInFolder();
            return TRUE;
        }
        else if (LOWORD(wParam) == ID_FOLDER_RENAME) {
            HTREEITEM treeItem = TreeView_GetSelection(hTree);
            if (!treeItem) {
				return TRUE;
            }
            TVITEM tvItem = getTreeItem(hTree, treeItem);
            optional<VFolder*> vFolderOpt = commonData.vData.findFolderByOrder((int)tvItem.lParam);
            if (!vFolderOpt) {
				return TRUE;
            }
            VFolder* vFolder = vFolderOpt.value();
            
            // Show rename dialog for the folder
            showRenameDialog(vFolder, treeItem, hTree);
            return TRUE;
        }
        else if (LOWORD(wParam) == ID_FILE_RENAME) {
            HTREEITEM treeItem = TreeView_GetSelection(hTree);
            if (!treeItem) {
				return TRUE;
            }
            TVITEM tvItem = getTreeItem(hTree, treeItem);
            optional<VFile*> vFileOpt = commonData.vData.findFileByOrder((int)tvItem.lParam);
            if (!vFileOpt) {
				return TRUE;
            }
            VFile* vFile = vFileOpt.value();
            
            // Show notepad++ rename dialog for the file
            UINT_PTR bufferID = npp(NPPM_GETBUFFERIDFROMPOS, vFile->docOrder, vFile->view);
            npp(NPPM_MENUCOMMAND, bufferID, IDM_FILE_RENAME);
            return TRUE;
        }
        else if (LOWORD(wParam) == ID_FOLDER_UNWRAP) {
            unwrapFolder();
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

            if (hDragItem && hDropTarget && hDragItem != hDropTarget) {
				TVITEM dropItem = getTreeItem(hTree, hDropTarget);
                if (dropItem.iImage == iconIndex[ICON_FOLDER]) {
                    // Move file into folder
                    int dragOrder = getOrderFromTreeItem(hTree, hDragItem);
                    int targetOrder = getOrderFromTreeItem(hTree, hDropTarget);
                    auto draggedFileOpt = commonData.vData.findFileByOrder(dragOrder);
                    auto draggedFolderOpt = commonData.vData.findFolderByOrder(dragOrder);
                    auto targetFolderOpt = commonData.vData.findFolderByOrder(targetOrder);

                    if (draggedFileOpt && targetFolderOpt) 
                    {
                        moveFileIntoFolder(dragOrder, targetOrder);
                    }
                    else if (draggedFolderOpt && targetFolderOpt) 
                    {
                        moveFolderIntoFolder(dragOrder, targetOrder);
                    }
                }
                // else if (dropItem.iImage == idxFile) { /* ignore file-to-file drops */ }
                else if (insertion) {
                    // Get the dragged and target item orders
                    int dragOrder = getOrderFromTreeItem(hTree, hDragItem);
                    int targetOrder = getOrderFromTreeItem(hTree, hDropTarget);

                    // Find the dragged item in vData
                    optional<VFile*> draggedFileOpt = commonData.vData.findFileByOrder(dragOrder);
                    optional<VFolder*> draggedFolderOpt = commonData.vData.findFolderByOrder(dragOrder);

                    if (draggedFileOpt) {
                        // Calculate new order based on drop position
                        int newOrder = calculateNewOrder(targetOrder, lastMark);
                        
                        // Reorder other items
                        reorderItems(dragOrder, newOrder);
                        writeJsonFile();
                    }
                    else if (draggedFolderOpt) {
                        // Similar logic for folders
                        int newOrder = calculateNewOrder(targetOrder, lastMark);
                        reorderFolders(dragOrder, newOrder);
                        writeJsonFile();
                    }
                }
                commonData.vData.vDataSort();
                
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

void unwrapFolder()
{
    HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);
    HTREEITEM treeItem = TreeView_GetSelection(hTree);
    if (!treeItem) {
        return;
    }
    TVITEM tvItem = getTreeItem(hTree, treeItem);
    optional<VFolder*> vFolderOpt = commonData.vData.findFolderByOrder((int)tvItem.lParam);
    if (!vFolderOpt) {
        return;
    }
    VFolder* vFolder = vFolderOpt.value();
	VFolder folderCopy = *vFolder;
    HTREEITEM folderItemToDelete = folderCopy.hTreeItem;
    VFolder* parentFolder = commonData.vData.findParentFolder(vFolder->getOrder());
	int parentFolderOrder = parentFolder ? parentFolder->getOrder() : -1;

    size_t pos = folderCopy.getOrder();
    HTREEITEM fileTreeItem = nullptr;
    optional<VBase*> aboveSibling = parentFolder ? parentFolder->findAboveSibling(pos) : commonData.vData.findAboveSibling(pos);
    if (aboveSibling) {
        fileTreeItem = aboveSibling.value()->hTreeItem;
    }
    else {
        fileTreeItem = TVI_FIRST;
    }



    if (parentFolder) {
        parentFolder->removeChild(vFolder->getOrder());
		parentFolder = commonData.vData.findFolderByOrder(parentFolderOrder).value();
        auto children = folderCopy.getAllChildren();
        parentFolder->addChildren(children);
    }
    else {
        commonData.vData.removeChild(vFolder->getOrder());
        auto children = folderCopy.getAllChildren();
        commonData.vData.addChildren(children);
	}
    adjustGlobalOrdersForFileMove(folderCopy.getOrder(), commonData.vData.getLastOrder() + 1);
    TreeView_DeleteItem(hTree, folderCopy.hTreeItem);

    if (folderCopy.name == "New Folder 3") {
		//return; // Prevents crash when unwrapping "New Folder 3"
    }

    BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);
    folderCopy.move(-1);
    vector<VBase*> allChildren = folderCopy.getAllChildren();
    for (const auto& child : allChildren) {
        if (!child) continue;
        if (auto file = dynamic_cast<VFile*>(child)) {
            fileTreeItem = addFileToTree(file, hTree, parentFolder ? parentFolder->hTreeItem : nullptr, isDarkMode, fileTreeItem);
            pos++;
        } else if (auto folder = dynamic_cast<VFolder*>(child)) {
            fileTreeItem = addFolderToTree(folder, hTree, parentFolder ? parentFolder->hTreeItem : nullptr, pos, fileTreeItem);
		}
    }
}

void wrapFileInFolder()
{
    HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);
    HTREEITEM hSel = TreeView_GetSelection(hTree);
    if (!hSel) {
        return;
    }
    TVITEM item = getTreeItem(hTree, hSel);
    optional<VFile*> vFileOpt = commonData.vData.findFileByOrder((int)item.lParam);
    if (!vFileOpt) {
        return;
    }

    VFile* vFile = vFileOpt.value();
    VFile fileCopy = *vFile; // Create a copy of VFile*

    int oldOrder = vFile->getOrder();
    int lastOrder = commonData.vData.getLastOrder();

    VFolder* parentFolder = commonData.vData.findParentFolder(vFile->getOrder());
    if (parentFolder) {
        parentFolder->removeFile(vFile->getOrder());
    }
    else {
        commonData.vData.removeFile(vFile->getOrder());
    }
    TreeView_DeleteItem(hTree, hSel);
    commonData.vData.adjustOrders(oldOrder, NULL, 1);


    // Create new folder
    VFolder newFolder;
    newFolder.name = "New Folder";
    newFolder.setOrder(oldOrder);
    if (parentFolder) {
        parentFolder->folderList.push_back(newFolder);
    }
    else {
        commonData.vData.folderList.push_back(newFolder);
    }
    optional<VFolder*> newFolderOpt = commonData.vData.findFolderByOrder(oldOrder);

    newFolderOpt.value()->addFile(&fileCopy);
    newFolderOpt.value()->isExpanded = true;
    size_t pos = oldOrder;

    /*HTREEITEM folderTreeItem = addFolderToTree(newFolderOpt.value(), hTree, parentFolder ? parentFolder->hTreeItem : nullptr, pos, TVI_LAST);
    TreeView_DeleteItem(hTree, folderTreeItem);*/
    HTREEITEM folderTreeItem = nullptr;
    pos = oldOrder;
    optional<VBase*> aboveSiblingOpt = parentFolder ? parentFolder->findAboveSibling(oldOrder) : commonData.vData.findAboveSibling(oldOrder);
    if (aboveSiblingOpt) {
        folderTreeItem = addFolderToTree(
            newFolderOpt.value(),
            hTree,
            parentFolder ? parentFolder->hTreeItem : nullptr,
            pos,
            aboveSiblingOpt.value()->hTreeItem
        );
    }
    else {
        folderTreeItem = addFolderToTree(newFolderOpt.value(), hTree, parentFolder ? parentFolder->hTreeItem : nullptr, pos, TVI_FIRST);
    }
}

void treeItemSelected(HTREEITEM selectedTreeItem)
{
    HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);
    // Handle file selection - open file when user clicks on a file item
    if (!selectedTreeItem) {
        return;
    }
    TVITEM item = { 0 };
    item.mask = TVIF_IMAGE | TVIF_PARAM;
    item.hItem = selectedTreeItem;
    if (!TreeView_GetItem(hTree, &item)) {
        return;
    }
    // Check if this is a folder (not a file) by checking the image index
    if (item.iImage == iconIndex[ICON_FOLDER]) {
        return;
    }

    // This is a file item, get the order and find the corresponding VFile
    int order = static_cast<int>(item.lParam);

    // Find the VFile using the helper function
    optional<VFile*> selectedFile = commonData.vData.findFileByOrder(order);

    // If file found and has a valid path, open it
    if (selectedFile && !selectedFile.value()->path.empty()) {
        std::wstring wideName(selectedFile.value()->name.begin(), selectedFile.value()->name.end()); // opens by name. not path. With path it opens as a new file
        std::wstring widePath = std::wstring(selectedFile.value()->path.begin(), selectedFile.value()->path.end()); // opens by path.
        if (selectedFile.value()->backupFilePath.empty()) {
            wideName = std::wstring(selectedFile.value()->path.begin(), selectedFile.value()->path.end()); // opens by path.
        }

        LOG("Opening file: [{}]", selectedFile.value()->path);

        // First, try to switch to the file if it's already open
        ignoreSelectionChange = true;
        if (!npp(NPPM_SWITCHTOFILE, 0, reinterpret_cast<LPARAM>(wideName.c_str()))) {
            // If file is not open, open it
            npp(NPPM_DOOPEN, 0, reinterpret_cast<LPARAM>(wideName.c_str()));
        }

        // Optionally, switch to the appropriate view if specified
        // Switch to the specified view (0 = main view, 1 = sub view)
        if (selectedFile.value()->view >= 0 && selectedFile.value()->view != currentView) {
            npp(NPPM_ACTIVATEDOC, selectedFile.value()->view, selectedFile.value()->docOrder);
            currentView = selectedFile.value()->view;
        }
    }
    else if (selectedFile && selectedFile.value()->path.empty()) {
        OutputDebugStringA("File has empty path, cannot open");
    }
    else {
        OutputDebugStringA("File not found in vData");
    }
}

void moveFileIntoFolder(int dragOrder, int targetOrder) {
    auto draggedFileOpt = commonData.vData.findFileByOrder(dragOrder);
    auto targetFolderOpt = commonData.vData.findFolderByOrder(targetOrder);
    VFile* file = draggedFileOpt.value();
    VFolder* folder = targetFolderOpt.value();
    VFolder* parentFolder = commonData.vData.findParentFolder(file->getOrder());
    VFile fileCopy = *file; // Create a copy of VFile*


    HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);

    // Remove dragged item from tree
    TreeView_DeleteItem(hTree, hDragItem);
    // Add dragged item as child of target folder in tree
    addFileToTree(&fileCopy, hTree, hDropTarget, npp(NPPM_ISDARKMODEENABLED, 0, 0), TVI_LAST);


    if (parentFolder) {
        parentFolder->removeFile(file->getOrder());
    }
    else {
        commonData.vData.removeFile(file->getOrder());
    }
    commonData.vData.adjustOrders(fileCopy.getOrder(), NULL, -1);
    // 
    folder->addFile(&fileCopy);
    int tempOrder = fileCopy.getOrder();
    commonData.vData.adjustOrders(fileCopy.getOrder(), INT_MAX, 1);
    // Update the file's order to match the folder's order
    file = commonData.vData.findFileByPath(fileCopy.path);
    file->setOrder(tempOrder);

    writeJsonFile();

    //OutputDebugStringA("%s moved in to folder %s's end\n");

    LOG("[{}] moved into folder [{}]'s end", file->name, folder->name);
}

void moveFolderIntoFolder(int dragOrder, int targetOrder) {
    // TODO: folder into folder
    auto draggedFolderOpt = commonData.vData.findFolderByOrder(dragOrder);
    auto targetFolderOpt = commonData.vData.findFolderByOrder(targetOrder);

    VFolder* movedFolder = draggedFolderOpt.value();
    VFolder* targetFolder = targetFolderOpt.value();

    int step = targetFolder->getLastOrder() - dragOrder + 1;
    if (dragOrder < targetOrder) {
        step = step - movedFolder->countItemsInFolder();
    }
    VFolder movedFolderCopy = *movedFolder;
	
    
    optional<VFolder*> sourceParentFolder = commonData.vData.findParentFolder(dragOrder);
    if (sourceParentFolder.value() != nullptr) {
        sourceParentFolder.value()->removeChild(dragOrder);
    }
    else {
		commonData.vData.removeChild(dragOrder);
    }
    targetFolderOpt = commonData.vData.findFolderByOrder(targetOrder); // removeChild operation somehow effected targetFolder
    targetFolder = targetFolderOpt.value();

    size_t pos = targetFolder->getLastOrder();
	adjustGlobalOrdersForFolderMove(movedFolderCopy.getOrder(), pos + 1, movedFolderCopy.countItemsInFolder());
    movedFolderCopy.move(step);
    targetFolder->folderList.push_back(movedFolderCopy);
    movedFolder = commonData.vData.findFolderByOrder(movedFolderCopy.getOrder()).value();
    
	pos = movedFolder->getOrder();
    HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);
    
    HTREEITEM folderTreeItem = addFolderToTree(movedFolder, hTree, targetFolder->hTreeItem, pos, TVI_LAST);

	TreeView_DeleteItem(hTree, hDragItem);
}

HTREEITEM addFileToTree(VFile* vFile, HWND hTree, HTREEITEM hParent, bool darkMode, HTREEITEM hPrevItem) {
    wchar_t buffer[100];

    if (vFile->name.find("\\") != std::string::npos) {
		OutputDebugStringA(("Invalid file name: " + vFile->name + "\n").c_str());
    }

    TVINSERTSTRUCT tvis = { 0 };
    tvis.hParent = hParent;
    tvis.hInsertAfter = hPrevItem;
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
    wcscpy_s(buffer, 100, toWchar(vFile->name));
    tvis.item.pszText = buffer;
    if (vFile->isEdited) {
        tvis.item.iImage = iconIndex[ICON_FILE_EDITED]; // Use edited icon
        tvis.item.iSelectedImage = iconIndex[ICON_FILE_EDITED]; // Use edited icon
    }
    else if (darkMode){
        tvis.item.iImage = iconIndex[ICON_FILE_DARK];
        tvis.item.iSelectedImage = iconIndex[ICON_FILE_DARK];
    }
    else {
        tvis.item.iImage = iconIndex[ICON_FILE_LIGHT]; // Use light file icon
        tvis.item.iSelectedImage = iconIndex[ICON_FILE_LIGHT]; // Use light file icon
    }
	tvis.item.lParam = vFile->getOrder(); // Store the order/index directly in lparam

    HTREEITEM hItem = TreeView_InsertItem(hTree, &tvis);
	vFile->hTreeItem = hItem; // Store the HTREEITEM in the VFile for later reference

    if (vFile->isActive) {
        TreeView_SelectItem(hTree, hItem);
        TreeView_EnsureVisible(hTree, hItem);
    }

	return hItem;
}

HTREEITEM addFolderToTree(VFolder* vFolder, HWND hTree, HTREEITEM hParent, size_t& pos, HTREEITEM prevItem) {
    wchar_t buffer[100];

    TVINSERTSTRUCT tvis = { 0 };
    tvis.hParent = hParent;
    tvis.hInsertAfter = prevItem;
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
    wcscpy_s(buffer, 100, toWchar(vFolder->name));
    tvis.item.pszText = buffer;
    tvis.item.iImage = iconIndex[ICON_FOLDER]; // or idxFile
    tvis.item.iSelectedImage = iconIndex[ICON_FOLDER]; // or idxFile
    tvis.item.lParam = vFolder->getOrder(); // Store the order/index directly in lparam

    // Free previous hTreeItem if it exists
    if (vFolder->hTreeItem) {
        // No explicit free needed for HTREEITEM, but we can clear it to avoid dangling references
        vFolder->hTreeItem = nullptr;
    }

    HTREEITEM hFolder = TreeView_InsertItem(hTree, &tvis);
    vFolder->hTreeItem = hFolder; // Store the HTREEITEM in the VFolder for later reference

	prevItem = hFolder;

    pos++;
    BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);

    int lastOrder = vFolder->fileList.empty() ? 0 : vFolder->fileList.back().getOrder();
    lastOrder = std::max(lastOrder, vFolder->folderList.empty() ? 0 : vFolder->folderList.back().getOrder());
    for (pos; pos <= lastOrder; pos) {
        optional<VFile*> vFile = vFolder->findFileByOrder(pos);
        if (vFile) {
            prevItem = addFileToTree(*vFile, hTree, hFolder, isDarkMode, prevItem);
            pos++;
        }
        else {
            auto vSubFolder = vFolder->findFolderByOrder(pos);
            if (vSubFolder) {
                prevItem = addFolderToTree(*vSubFolder, hTree, hFolder, pos, prevItem);
            }
        }
    }

    if (vFolder->isExpanded) {
        TreeView_Expand(hTree, hFolder, TVE_EXPAND);
    }
    else {
        TreeView_Expand(hTree, hFolder, TVE_COLLAPSE);
    }
    return hFolder;
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

void updateTreeColorsExternal(HWND hTree) {
    updateTreeColors(hTree);
}

// Helper functions
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

HTREEITEM FindItemByLParam(HWND hTree, HTREEITEM hParent, LPARAM lParam) {
    HTREEITEM hItem = TreeView_GetChild(hTree, hParent);
    while (hItem) {
        TVITEM tvi = { 0 };
        tvi.mask = TVIF_PARAM;
        tvi.hItem = hItem;
        if (TreeView_GetItem(hTree, &tvi) && tvi.lParam == lParam)
            return hItem;
        HTREEITEM hFound = FindItemByLParam(hTree, hItem, lParam);
        if (hFound) return hFound;
        hItem = TreeView_GetNextSibling(hTree, hItem);
    }
    return nullptr;
}

void reorderItems(int oldOrder, int newOrder) {
    // Find the file to move (could be in root or any folder)
    FileLocation sourceLocation = findFileLocation(commonData.vData, oldOrder);
    if (!sourceLocation.found || !sourceLocation.file) {
        return; // File not found
    }
    BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);

    
    VFile* movedFile = sourceLocation.file;
    VFolder* sourceFolder = sourceLocation.parentFolder;
    
    // For now, we'll implement a simpler approach that handles the most common cases:
    // 1. Moving within the same container (root or same folder)
    // 2. Moving from folder to root
    // 3. Moving from root to folder
    
    // Determine if we're moving to root level or staying in a folder
    bool movingToRoot = false;
    VFolder* targetFolder = nullptr;
    
    // Count items at root level to determine if target is root
    int rootItemCount = commonData.vData.fileList.size() + commonData.vData.folderList.size(); // TODO: add a class function if the order is in root
    
    if (commonData.vData.isInRoot(newOrder)) {
        movingToRoot = true;
        targetFolder = nullptr;
    } else {
        // For now, assume we're staying in the same folder
        // In a full implementation, you'd need to determine the exact target folder
        FileLocation targetLocation = findFileLocation(commonData.vData, newOrder);
		targetFolder = targetLocation.parentFolder;
        //targetFolder = sourceFolder;
    }
    
    // If moving to a different container, we need to handle the move
    if (movingToRoot && sourceFolder) {
        // Moving from folder to root
        // Store the file data before removing
        VFile fileData = *movedFile;
        
        // First, adjust global orders to make space for the move
        adjustGlobalOrdersForFileMove(oldOrder, newOrder);
        
        // Remove from source folder
        auto& sourceFileList = sourceFolder->fileList;
        sourceFileList.erase(std::remove_if(sourceFileList.begin(), sourceFileList.end(),
            [movedFile](const VFile& file) { return &file == movedFile; }), sourceFileList.end());
        
        // Set new order and add to root
		if (newOrder > oldOrder) newOrder--;
		fileData.setOrder(newOrder);
        commonData.vData.fileList.push_back(fileData);
        movedFile = commonData.vData.findFileByOrder(newOrder).value();

        HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);
        HTREEITEM oldItem = fileData.hTreeItem;
        HTREEITEM prevItem = nullptr;
        optional<VBase*> aboveSibling = commonData.vData.findAboveSibling(newOrder);
        if (aboveSibling) {
            prevItem = aboveSibling.value()->hTreeItem;
            LOG("[{}] moved into root after [{}]", movedFile->name, aboveSibling.value()->name);
        }
        else {
            prevItem = TVI_FIRST;
            LOG("[{}] moved into root to first place", movedFile->name);
        }

        TreeView_DeleteItem(hTree, fileData.hTreeItem);
        addFileToTree(movedFile, hTree, nullptr, isDarkMode, prevItem);
        
        
    } else if (!movingToRoot && !sourceFolder) {
        // Moving from root to folder
        // Store the file data before removing
        VFile fileData = *movedFile;
        
        // First, adjust global orders to make space for the move
        adjustGlobalOrdersForFileMove(oldOrder, newOrder);
        
        // Remove from root
        auto& rootFileList = commonData.vData.fileList;
        rootFileList.erase(std::remove_if(rootFileList.begin(), rootFileList.end(),
            [movedFile](const VFile& file) { return &file == movedFile; }), rootFileList.end());

		if (newOrder > oldOrder) newOrder--;
        // Find the target folder (simplified - would need proper logic)
        // For now, we'll assume the first folder
        if (!commonData.vData.folderList.empty()) {
            // Set new order and add to folder
			fileData.setOrder(newOrder);
            targetFolder->fileList.push_back(fileData);
            movedFile = targetFolder->findFileByOrder(newOrder).value();
        }

        HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);
        HTREEITEM oldItem = nullptr;
        HTREEITEM parentItem = targetFolder->hTreeItem;
        HTREEITEM prevItem = nullptr;
        optional<VBase*> aboveSibling = targetFolder->findAboveSibling(newOrder);
        if (aboveSibling) {
            prevItem = aboveSibling.value()->hTreeItem;
            LOG("[{}] moved into folder [{}] after [{}]", movedFile->name, targetFolder->name, aboveSibling.value()->name);
        } else {
            prevItem = TVI_FIRST;
            LOG("[{}] moved into folder [{}]'s start", movedFile->name, targetFolder->name);
		}

        /*oldItem = FindItemByLParam(hTree, nullptr, (LPARAM)movedFile->getOrder());*/
        TreeView_DeleteItem(hTree, fileData.hTreeItem);

        addFileToTree(movedFile, hTree, parentItem, isDarkMode, prevItem);
        
        
    } else {
        // Moving within the same container (root or same folder)
        // Since order is global across the entire tree, we need to adjust all affected items globally
        
        
        // Set the new order for the moved file
        if (newOrder > oldOrder) newOrder--;
        //newOrder--;
        if (newOrder == oldOrder) return;
        

        //HTREEITEM FindItemByLParam(HWND hTree, HTREEITEM hParent, LPARAM lParam);
        HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);
        HTREEITEM oldItem = nullptr;
        HTREEITEM hParent = nullptr;
		HTREEITEM prevItem = nullptr;

        int firstOrderOfFolder = -1;
        if (sourceFolder) {
			hParent = FindItemByLParam(hTree, nullptr, (LPARAM)sourceFolder->getOrder());
			firstOrderOfFolder = sourceFolder->getOrder() + 1;
        } else {
			firstOrderOfFolder = commonData.vData.fileList[0].getOrder();
        }
        TVINSERTSTRUCT tvis = { 0 };

		// What if newOrder is the first item in the folder?
        if (newOrder == firstOrderOfFolder) {
            tvis.hInsertAfter = TVI_FIRST;
        } else {
            prevItem = FindItemByLParam(hTree, hParent, (LPARAM)newOrder);
            if (newOrder < oldOrder) {
                prevItem = FindItemByLParam(hTree, hParent, (LPARAM)newOrder - 1);
            }
            tvis.hInsertAfter = prevItem;
		}
        oldItem = FindItemByLParam(hTree, hParent, (LPARAM)movedFile->getOrder());
        TreeView_DeleteItem(hTree, oldItem);

        adjustGlobalOrdersForFileMove(oldOrder, newOrder > oldOrder ? newOrder+1 : newOrder);
		movedFile->setOrder(newOrder);

		addFileToTree(movedFile, hTree, hParent, isDarkMode, prevItem);

        LOG("[{}] moved to order [{}]", movedFile->name, newOrder);

    }
}

void reorderFolders(int oldOrder, int newOrder) {
    // Find the folder to move using recursive search
    FolderLocation moveLocation = findFolderLocation(commonData.vData, oldOrder);

    if (!moveLocation.found) {
        return;  // Folder not found
    }

    HWND hTree = GetDlgItem(watcherPanel, IDC_TREE1);
    optional<VFolder*> movedFolderOpt = commonData.vData.findFolderByOrder(oldOrder);
	VFolder* movedFolder = movedFolderOpt.value();
    VFolder* sourceParentFolder = moveLocation.parentFolder;
    VFolder* targetParentFolder = commonData.vData.findParentFolder(newOrder);


    // Count total items in the folder (folder itself + all contents recursively)
    int folderItemCount = (*movedFolder).countItemsInFolder();
       

    // Tree manipulation - similar to reorderItems
    HTREEITEM oldItem = movedFolder->hTreeItem;
    HTREEITEM sourceParentItem = nullptr;
    HTREEITEM targetParentItem = nullptr;
    HTREEITEM prevItem = nullptr;

    
    sourceParentItem = sourceParentFolder ? sourceParentFolder->hTreeItem : nullptr;
    targetParentItem = targetParentFolder ? targetParentFolder->hTreeItem : nullptr;

    // Find the insertion point
    int firstOrderOfFolder = -1;
    if (targetParentFolder) {
        firstOrderOfFolder = targetParentFolder->getOrder() + 1;
    } else {
        // Find first order at root level
        if (!commonData.vData.fileList.empty()) {
            firstOrderOfFolder = commonData.vData.fileList[0].getOrder();
        } else if (!commonData.vData.folderList.empty()) {
            firstOrderOfFolder = commonData.vData.folderList[0].getOrder();
        }
    }

    if (newOrder == firstOrderOfFolder) {
        prevItem = TVI_FIRST;
    } else {
        // Find the item that should come before the moved folder
        optional<VBase*> aboveSibling = commonData.vData.findAboveSibling(newOrder);
        if (aboveSibling) {
            prevItem = aboveSibling.value()->hTreeItem;
        } else {
            prevItem = TVI_FIRST;
        }
    }

    TreeView_DeleteItem(hTree, oldItem);

    
	VFolder movedFolderCopy = *movedFolder;
    if (sourceParentFolder) {
        sourceParentFolder->removeChild(oldOrder);
    }
    else {
        commonData.vData.removeChild(oldOrder);
	}

    targetParentFolder = commonData.vData.findParentFolder(newOrder);
    targetParentItem = targetParentFolder ? targetParentFolder->hTreeItem : nullptr;

    if (targetParentFolder) {
        targetParentFolder->folderList.push_back(movedFolderCopy);
    }
    else {
        commonData.vData.folderList.push_back(movedFolderCopy);
	}
	movedFolder = commonData.vData.findFolderByOrder(movedFolderCopy.getOrder()).value();

    // Adjust global orders to make space for the moved folder and its contents
    adjustGlobalOrdersForFolderMove(oldOrder, newOrder, folderItemCount);

    if (newOrder > oldOrder) {
        newOrder = newOrder - movedFolder->countItemsInFolder();
    }

    // Set the new order for the moved folder
    movedFolder->move(newOrder - oldOrder);


    // Re-insert the folder in the new position
    // addFolderToTree will recursively add all files and subfolders
    size_t pos = newOrder;
    HTREEITEM newItem = addFolderToTree(movedFolder, hTree, targetParentItem, pos, prevItem);

    // After reordering, recursively sort the entire data structure to ensure consistency
    commonData.vData.vDataSort();
}

FileLocation findFileLocation(VData& vData, int order) {
    FileLocation location;
    // TODO: change the folder location according to insertion mark 


    // Check root level first
    for (auto& file : vData.fileList) {
        if (file.getOrder() == order) {
            location.file = &file;
            location.parentFolder = nullptr;  // Root level
            location.found = true;
            return location;
        }
    }

    // Recursively search in folders
    std::function<bool(VFolder&)> searchInFolder = [&](VFolder& folder) -> bool {
        // Check files in this folder
        for (auto& file : folder.fileList) {
            if (file.getOrder() == order) {
                location.file = &file;
                location.parentFolder = &folder;
                location.found = true;
                return true;
            }
        }

        // Recursive search in subfolders
        for (auto& subFolder : folder.folderList) {
            if (searchInFolder(subFolder)) {
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

void changeTreeItemIcon(UINT_PTR bufferID) {
    BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);
    auto position = npp(NPPM_GETPOSFROMBUFFERID, bufferID, 0);
	optional<VFile*> vFileOpt = commonData.vData.findFileByDocOrder((int)position);
    if (!vFileOpt) {
        return;
	}

    TVITEM item = { 0 };
    item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    item.hItem = vFileOpt.value()->hTreeItem;

    wchar_t* name = toWchar(vFileOpt.value()->name);
    item.pszText = name;

    if (!commonData.bufferStates[bufferID] || vFileOpt.value()->isEdited) {
        item.iImage = iconIndex[ICON_FILE_EDITED]; // Use edited icon
        item.iSelectedImage = iconIndex[ICON_FILE_EDITED]; // Use edited icon
    }
    else if (isDarkMode) {
        item.iImage = iconIndex[ICON_FILE_DARK];
        item.iSelectedImage = iconIndex[ICON_FILE_DARK];
    }
    else {
        item.iImage = iconIndex[ICON_FILE_LIGHT]; // Use light file icon
        item.iSelectedImage = iconIndex[ICON_FILE_LIGHT]; // Use light file icon
    }


    TreeView_SetItem(commonData.hTree, &item);
}