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
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")

#pragma comment(lib, "comctl32.lib")

// External variables
extern CommonData commonData;

extern NPP::FuncItem menuDefinition[];  // Defined in Plugin.cpp
extern int menuItem_ToggleVirtualPanel;      // Defined in Plugin.cpp
extern int menuItem_IncreaseFont;      // Defined in Plugin.cpp
extern int menuItem_DecreaseFont;      // Defined in Plugin.cpp
extern int menuItem_ShortcutOverrider;   // Defined in Plugin.cpp

extern bool checkRootVFolderJSON(); // Defined in VirtualPanel.cpp


HMENU hContextMenu = nullptr;
HMENU fontSizeSubMenu = nullptr;
HMENU fileContextMenu = nullptr;
HMENU openContextSubMenu = nullptr;
HMENU copyContextSubMenu = nullptr;
HMENU moveContextSubMenu = nullptr;
HMENU folderContextMenu = nullptr;




std::unordered_map<IconType, int> iconIndex;

DialogStretch stretch;

static HBRUSH hBgBrush = NULL;

WNDPROC oldTreeProc;

static HTREEITEM hHoveredItem = nullptr;


// TreeView subclass procedure
LRESULT CALLBACK TreeView_SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HTREEITEM lastHovered = nullptr;
    switch (msg) {
    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        TVHITTESTINFO hit = { 0 };
        hit.pt = pt;
        HTREEITEM hItem = TreeView_HitTest(hwnd, &hit);

        if (hHoveredItem != hItem) {
            // Invalidate only the old and new hovered items
            if (hHoveredItem) {
                RECT rc;
                if (TreeView_GetItemRect(hwnd, hHoveredItem, &rc, FALSE))
                    InvalidateRect(hwnd, &rc, FALSE);
            }
            if (hItem) {
                RECT rc;
                if (TreeView_GetItemRect(hwnd, hItem, &rc, FALSE))
                    InvalidateRect(hwnd, &rc, FALSE);
            }
            hHoveredItem = hItem;
        }

        // Track mouse leave
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        break;
    }
    case WM_MOUSELEAVE: {
        if (hHoveredItem) {
            RECT rc;
            if (TreeView_GetItemRect(hwnd, hHoveredItem, &rc, FALSE))
                InvalidateRect(hwnd, &rc, FALSE);
            hHoveredItem = nullptr;
        }
        break;
    }
    case WM_COMMAND:
    {
		// It does not work here. Event goes to subclass procedure but not here.
        switch (LOWORD(wParam))
        {
        case IDM_VIEW_TAB_PREV:
            // Ctrl+PgUp : your own behavior
            LOG("Keyboard Shortcut: [{}]", IDM_VIEW_TAB_PREV);
            return 0; // swallow so NPP doesn’t process it

        case IDM_VIEW_TAB_NEXT:
            // Ctrl+PgDn : your own behavior
            LOG("Keyboard Shortcut: [{}]", IDM_VIEW_TAB_NEXT);
            return 0; // swallow so NPP doesn’t process it
        }
    }
    }
    return CallWindowProc(oldTreeProc, hwnd, msg, wParam, lParam);
}




// Forward declarations
void writeJsonFile();
void updateVirtualPanelUnconditional(UINT_PTR bufferID);


LRESULT nppMenuCall(HTREEITEM selectedTreeItem, int MENU_ID);
void treeItemSelected(HTREEITEM selectedTreeItem);
void moveFileIntoFolder(int dragOrder, int targetOrder);
void moveFolderIntoFolder(int dragOrder, int targetOrder);
void unwrapFolder(HTREEITEM selectedTreeItem);
void wrapFileInFolder(HTREEITEM selectedTreeItem);




// External variables
extern HWND virtualPanelWnd;



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

void loadMenus() {

	// This does not work for some reason.
    static std::wstring pluginTitleStr;
    pluginTitleStr = commonData.translator->getTextW("IDM_PLUGIN_TITLE");
    static const wchar_t* pluginTitle = pluginTitleStr.c_str();

	dock.pszName = pluginTitle;  // title bar text (caption in dialog is replaced)

    ::SendMessage(plugin.nppData._nppHandle, NPPM_DMMUPDATEDISPINFO, 0, (LPARAM)virtualPanelWnd);





    // Create context menu
    if (hContextMenu) {
        ::DestroyMenu(hContextMenu);
        hContextMenu = nullptr;
    }
    hContextMenu = CreatePopupMenu();
    //AppendMenu(hContextMenu, MF_STRING, MENU_ID_TREE_DELETE, L"Delete");

    if (fontSizeSubMenu) {
        ::DestroyMenu(fontSizeSubMenu);
        fontSizeSubMenu = nullptr;
    }
    fontSizeSubMenu = CreatePopupMenu();
    std::wstring fontIncreaseLabel = commonData.translator->getTextW("IDM_FONT_INCREASE") + std::to_wstring(commonData.fontSize) + L" px";
    AppendMenu(fontSizeSubMenu, MF_STRING, MENU_ID_INCREASEFONT, fontIncreaseLabel.c_str());
    std::wstring fontDecreaseLabel = commonData.translator->getTextW("IDM_FONT_DECREASE") + std::to_wstring(commonData.fontSize) + L" px";
    AppendMenu(fontSizeSubMenu, MF_STRING, MENU_ID_DECREASEFONT, fontDecreaseLabel.c_str());
    AppendMenu(hContextMenu, MF_POPUP, (UINT_PTR)fontSizeSubMenu, L"Font Size");


    if (fileContextMenu) {
        ::DestroyMenu(fileContextMenu);
        fileContextMenu = nullptr;
    }
    fileContextMenu = CreatePopupMenu();
    AppendMenu(fileContextMenu, MF_STRING, MENU_ID_FILE_CLOSE, commonData.translator->getTextW("MENU_ID_FILE_CLOSE").c_str());
    AppendMenu(fileContextMenu, MF_STRING, MENU_ID_FILE_WRAP_IN_FOLDER, commonData.translator->getTextW("MENU_ID_FILE_WRAP_IN_FOLDER").c_str());
    AppendMenu(fileContextMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(fileContextMenu, MF_STRING, IDM_FILE_SAVE, commonData.translator->getTextW("IDM_FILE_SAVE").c_str());
    AppendMenu(fileContextMenu, MF_STRING, IDM_FILE_SAVEAS, commonData.translator->getTextW("IDM_FILE_SAVEAS").c_str());


    if (openContextSubMenu) {
        ::DestroyMenu(openContextSubMenu);
        openContextSubMenu = nullptr;
    }
    openContextSubMenu = CreatePopupMenu();
    AppendMenu(openContextSubMenu, MF_STRING, IDM_FILE_OPEN_FOLDER, commonData.translator->getTextW("IDM_FILE_OPEN_FOLDER").c_str());
    AppendMenu(openContextSubMenu, MF_STRING, IDM_FILE_OPEN_CMD, commonData.translator->getTextW("IDM_FILE_OPEN_CMD").c_str());
    AppendMenu(openContextSubMenu, MF_STRING, IDM_FILE_CONTAININGFOLDERASWORKSPACE, commonData.translator->getTextW("IDM_FILE_CONTAININGFOLDERASWORKSPACE").c_str());
    //EnableMenuItem(openContextSubMenu, MENU_ID_FILE_OPEN_PARENT_WORKSPACE, MF_BYCOMMAND | MF_DISABLED);
    AppendMenu(openContextSubMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(openContextSubMenu, MF_STRING, IDM_FILE_OPEN_DEFAULT_VIEWER, commonData.translator->getTextW("IDM_FILE_OPEN_DEFAULT_VIEWER").c_str());
    AppendMenu(fileContextMenu, MF_POPUP, (UINT_PTR)openContextSubMenu, commonData.translator->getTextW("IDM_OPEN_CONTEXT_MENU").c_str());


    AppendMenu(fileContextMenu, MF_STRING, IDM_FILE_RENAME, commonData.translator->getTextW("IDM_FILE_RENAME").c_str());
    AppendMenu(fileContextMenu, MF_STRING, IDM_FILE_DELETE, commonData.translator->getTextW("IDM_FILE_DELETE").c_str());
    AppendMenu(fileContextMenu, MF_STRING, IDM_FILE_RELOAD, commonData.translator->getTextW("IDM_FILE_RELOAD").c_str());
    AppendMenu(fileContextMenu, MF_STRING, IDM_FILE_PRINT, commonData.translator->getTextW("IDM_FILE_PRINT").c_str());
    AppendMenu(fileContextMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(fileContextMenu, MF_STRING, IDM_EDIT_TOGGLEREADONLY, commonData.translator->getTextW("IDM_EDIT_TOGGLEREADONLY").c_str());
    //AppendMenu(fileContextMenu, MF_STRING, IDM_FILE_PRINT, L"Read-Only Attribute in Windows");
    AppendMenu(fileContextMenu, MF_SEPARATOR, 0, NULL);

    copyContextSubMenu = CreatePopupMenu();
    AppendMenu(copyContextSubMenu, MF_STRING, IDM_EDIT_FULLPATHTOCLIP, commonData.translator->getTextW("IDM_EDIT_FULLPATHTOCLIP").c_str());
    AppendMenu(copyContextSubMenu, MF_STRING, IDM_EDIT_FILENAMETOCLIP, commonData.translator->getTextW("IDM_EDIT_FILENAMETOCLIP").c_str());
    AppendMenu(copyContextSubMenu, MF_STRING, IDM_EDIT_CURRENTDIRTOCLIP, commonData.translator->getTextW("IDM_EDIT_CURRENTDIRTOCLIP").c_str());
    AppendMenu(fileContextMenu, MF_POPUP, (UINT_PTR)copyContextSubMenu, commonData.translator->getTextW("IDM_CLIPBOARD").c_str());



    if (moveContextSubMenu) {
        ::DestroyMenu(moveContextSubMenu);
        moveContextSubMenu = nullptr;
    }
    moveContextSubMenu = CreatePopupMenu();
    AppendMenu(moveContextSubMenu, MF_STRING, IDM_VIEW_GOTO_ANOTHER_VIEW, commonData.translator->getTextW("IDM_VIEW_GOTO_ANOTHER_VIEW").c_str());
    AppendMenu(moveContextSubMenu, MF_STRING, IDM_VIEW_CLONE_TO_ANOTHER_VIEW, commonData.translator->getTextW("IDM_VIEW_CLONE_TO_ANOTHER_VIEW").c_str());
    AppendMenu(fileContextMenu, MF_POPUP, (UINT_PTR)moveContextSubMenu, commonData.translator->getTextW("IDM_MOVE_MENU").c_str());


    /**********************************************************************************************************************/
    if (folderContextMenu) {
        ::DestroyMenu(folderContextMenu);
        folderContextMenu = nullptr;
    }
    folderContextMenu = CreatePopupMenu();
    AppendMenu(folderContextMenu, MF_STRING, MENU_ID_FOLDER_UNWRAP, commonData.translator->getTextW("MENU_ID_FOLDER_UNWRAP").c_str());
    AppendMenu(folderContextMenu, MF_STRING, MENU_ID_FOLDER_RENAME, commonData.translator->getTextW("MENU_ID_FOLDER_RENAME").c_str());




    /**********************************************************************************************************************/
    // Plugin Menu


    wchar_t buffer[256];
    HMENU pluginMenu;
    HMENU hMenu = (HMENU)npp(NPPM_GETMENUHANDLE, (WPARAM)NPPPLUGINMENU, (LPARAM)0);

    int pluginMenuIndex = 0;
    int pluginMenuSize = GetMenuItemCount(hMenu);
    for (pluginMenuIndex = 0; pluginMenuIndex < pluginMenuSize; pluginMenuIndex++) {
        int copied = GetMenuStringW(hMenu, pluginMenuIndex, buffer, _countof(buffer), MF_BYPOSITION);
        if (fromWchar(buffer) == "VirtualFolders") {  // TODO: change to dynamic plugin name ?????
            break;
        }
    }
    if (pluginMenuIndex == pluginMenuSize) return;

    pluginMenu = GetSubMenu(hMenu, pluginMenuIndex);

    
    std::wstring shortcutMenuLabel = commonData.translator->getTextW("IDM_SHORTCUT_OVERRIDE");
    ModifyMenuW(pluginMenu, menuItem_ShortcutOverrider, MF_BYPOSITION | MF_STRING | plugin.isShortcutOverridden & MF_CHECKED, menuDefinition[menuItem_ShortcutOverrider]._cmdID, (LPCWSTR)(shortcutMenuLabel.c_str()));
    ::SendMessage(plugin.nppData._nppHandle, NPPM_SETMENUITEMCHECK, menuDefinition[menuItem_ShortcutOverrider]._cmdID, plugin.isShortcutOverridden ? 1 : 0);


    ModifyMenuW(pluginMenu, menuItem_IncreaseFont, MF_BYPOSITION | MF_STRING, menuDefinition[menuItem_IncreaseFont]._cmdID, (LPCWSTR)(fontIncreaseLabel.c_str()));
    ModifyMenuW(pluginMenu, menuItem_DecreaseFont, MF_BYPOSITION | MF_STRING, menuDefinition[menuItem_DecreaseFont]._cmdID, (LPCWSTR)(fontDecreaseLabel.c_str()));
}



// Add a new dialog procedure for the file view dialog (IDD_FILEVIEW)
INT_PTR CALLBACK fileViewDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hTree = nullptr;
    
    static InsertionMark lastMark = {};
    
    switch (uMsg) {
    case WM_TIMER:

        break;
    case WM_DESTROY:
        KillTimer(hwndDlg, 1); // clean up timer
        break;
    case WM_INITDIALOG: {
        stretch.setup(hwndDlg);
        hTree = GetDlgItem(hwndDlg, IDC_TREE1);

        oldTreeProc = (WNDPROC)SetWindowLongPtr(hTree, GWLP_WNDPROC, (LONG_PTR)TreeView_SubclassProc);
        SetWindowTheme(hTree, L"", L"");

        // initial color – match the current tree-view background
        //COLORREF bgColor = TreeView_GetBkColor(hTree);
        //hBgBrush = CreateSolidBrush(bgColor);


        //DWORD exStyle = TreeView_GetExtendedStyle(hTree);
        //TreeView_SetExtendedStyle(hTree, exStyle | TVS_EX_AUTOHSCROLL, TVS_EX_AUTOHSCROLL);


        loadMenus();




        HIMAGELIST hImages = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 2);
        //HICON hIconFolder = LoadIcon(NULL, IDI_APPLICATION);
        HICON hIconApp = LoadIcon(NULL, IDI_APPLICATION);
        HICON hIconFolder = LoadIcon(plugin.dllInstance, MAKEINTRESOURCE(IDI_FOLDER_YELLOW));
        HICON hIconFileLight = LoadIcon(plugin.dllInstance, MAKEINTRESOURCE(IDI_FILE_LIGHT_ICON));
        HICON hIconFileDark = LoadIcon(plugin.dllInstance, MAKEINTRESOURCE(IDI_FILE_DARK_ICON));
        HICON hIconFileEdited = LoadIcon(plugin.dllInstance, MAKEINTRESOURCE(IDI_FILE_EDITED_ICON));
        HICON hIconFileReadOnlyDark = LoadIcon(plugin.dllInstance, MAKEINTRESOURCE(IDI_FILE_LOCKED_DARK_ICON));
        HICON hIconFileReadOnlyLight = LoadIcon(plugin.dllInstance, MAKEINTRESOURCE(IDI_FILE_LOCKED_LIGHT_ICON));
        HICON hIconSecondaryViewIcon = LoadIcon(plugin.dllInstance, MAKEINTRESOURCE(IDI_FILE_VIEW_2_ICON));

        
        //HIMAGELIST stateImages = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 2);

        iconIndex[ICON_APP] = ImageList_AddIcon(hImages, hIconApp);
        iconIndex[ICON_FOLDER] = ImageList_AddIcon(hImages, hIconFolder);
        iconIndex[ICON_FILE_LIGHT] = ImageList_AddIcon(hImages, hIconFileLight);
        iconIndex[ICON_FILE_DARK] = ImageList_AddIcon(hImages, hIconFileDark);
        iconIndex[ICON_FILE_EDITED] = ImageList_AddIcon(hImages, hIconFileEdited);
        iconIndex[ICON_FILE_READONLY_DARK] = ImageList_AddIcon(hImages, hIconFileReadOnlyDark);
        iconIndex[ICON_FILE_READONLY_LIGHT] = ImageList_AddIcon(hImages, hIconFileReadOnlyLight);
        iconIndex[ICON_FILE_SECONDARY_VIEW] = ImageList_AddIcon(hImages, hIconSecondaryViewIcon);

        TreeView_SetImageList(hTree, hImages, TVSIL_NORMAL);
        TreeView_SetImageList(hTree, hImages, TVSIL_STATE);


        //TreeView_SetImageList(hTree, stateImages, TVSIL_STATE);




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
                    return TRUE;  // Skip processing if we are ignoring this change
				}


                COLORREF editorBg = (COLORREF)::SendMessage(plugin.currentScintilla(), SCI_STYLEGETBACK, STYLE_DEFAULT, 0);
                COLORREF editorFg = (COLORREF)::SendMessage(plugin.currentScintilla(), SCI_STYLEGETFORE, STYLE_DEFAULT, 0);
                TreeView_SetBkColor(hTree, editorBg);
                TreeView_SetTextColor(hTree, editorFg);
                //TreeView_SetLineColor(hTree, 0x99FF0000);



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
                HTREEITEM selectedTreeItem = TreeView_HitTest(hTree, &hit);

                //TVITEM tvItem = getTreeItem(hTree, selectedTreeItem);

                if (selectedTreeItem && (hit.flags & TVHT_ONITEM))
                {
                    TreeView_SelectItem(hTree, selectedTreeItem);

                    TVITEM item = { 0 };
                    item.mask = TVIF_IMAGE | TVIF_PARAM;
                    item.hItem = selectedTreeItem;
                    if (TreeView_GetItem(hTree, &item)) {
                        // Check if this is a folder (not a file) by checking the image index
                        if (item.iImage == iconIndex[ICON_FOLDER]) {
                            TrackPopupMenu(folderContextMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndDlg, NULL);
                        }
                        else {
                            optional<VFile*> vFileOpt = commonData.rootVFolder.findFileByOrder((int)item.lParam);
                            if (vFileOpt) {
                                VFile* vFile = vFileOpt.value();
                                if (vFile->backupFilePath.empty()) 
                                {
                                    EnableMenuItem(openContextSubMenu, IDM_FILE_OPEN_FOLDER, MF_BYCOMMAND | MF_ENABLED);
                                    EnableMenuItem(openContextSubMenu, IDM_FILE_OPEN_CMD, MF_BYCOMMAND | MF_ENABLED);
                                    EnableMenuItem(openContextSubMenu, IDM_FILE_CONTAININGFOLDERASWORKSPACE, MF_BYCOMMAND | MF_DISABLED);
                                    EnableMenuItem(openContextSubMenu, IDM_FILE_OPEN_DEFAULT_VIEWER, MF_BYCOMMAND | MF_ENABLED);
                                    EnableMenuItem(fileContextMenu, IDM_FILE_DELETE, MF_BYCOMMAND | MF_ENABLED);
                                    EnableMenuItem(fileContextMenu, IDM_FILE_RELOAD, MF_BYCOMMAND | MF_ENABLED);
                                    EnableMenuItem(fileContextMenu, IDM_FILE_PRINT, MF_BYCOMMAND | MF_ENABLED);
                                }
                                else 
                                {
                                    EnableMenuItem(openContextSubMenu, IDM_FILE_OPEN_FOLDER, MF_BYCOMMAND | MF_DISABLED);
                                    EnableMenuItem(openContextSubMenu, IDM_FILE_OPEN_CMD, MF_BYCOMMAND | MF_DISABLED);
                                    EnableMenuItem(openContextSubMenu, IDM_FILE_CONTAININGFOLDERASWORKSPACE, MF_BYCOMMAND | MF_DISABLED);
                                    EnableMenuItem(openContextSubMenu, IDM_FILE_OPEN_DEFAULT_VIEWER, MF_BYCOMMAND | MF_DISABLED);
                                    EnableMenuItem(fileContextMenu, IDM_FILE_DELETE, MF_BYCOMMAND | MF_DISABLED);
                                    EnableMenuItem(fileContextMenu, IDM_FILE_RELOAD, MF_BYCOMMAND | MF_DISABLED);
                                    EnableMenuItem(fileContextMenu, IDM_FILE_PRINT, MF_BYCOMMAND | MF_DISABLED);
                                }

                                CheckMenuItem(fileContextMenu, IDM_EDIT_TOGGLEREADONLY, MF_BYCOMMAND | (vFile->isReadOnly ? MF_CHECKED : MF_UNCHECKED));
                            }
                            TrackPopupMenu(fileContextMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndDlg, NULL);
                        }
					    contextMenuLoaded = true;
                    }

                }
                return TRUE;
            }
            case NM_CUSTOMDRAW: {
                LPNMTVCUSTOMDRAW tvcd = (LPNMTVCUSTOMDRAW)lParam;
				static int hoverItemIndex = 0;

                switch (tvcd->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;

                case CDDS_ITEMPREPAINT: {
                    HTREEITEM hItem = (HTREEITEM)tvcd->nmcd.dwItemSpec;
                    COLORREF editorFg = (COLORREF)::SendMessage(plugin.currentScintilla(), SCI_STYLEGETFORE, STYLE_DEFAULT, 0);
                    COLORREF editorBg = (COLORREF)::SendMessage(plugin.currentScintilla(), SCI_STYLEGETBACK, STYLE_DEFAULT, 0);

                    LOG("EditorBg: [{}]", editorBg);

                    COLORREF highlightBg = RGB(200, 220, 255);


                    RECT client;
                    GetClientRect(hTree, &client);
                    RECT rc;
                    TreeView_GetItemRect(hTree, hItem, &rc, FALSE);
                    rc.left = client.left;
                    rc.right = client.right;

                    HBRUSH hBrush = nullptr;


                    if (tvcd->nmcd.uItemState & CDIS_SELECTED) {
                        hBrush = CreateSolidBrush(RGB(152, 178, 227));
                        tvcd->clrText = RGB(255, 255, 255);
                        tvcd->clrTextBk = RGB(152, 178, 227);
                    }
                    else if (hItem == hHoveredItem) {
                        hBrush = CreateSolidBrush(highlightBg);
                        tvcd->clrText = editorFg;
                        tvcd->clrTextBk = highlightBg;
                    }
                    else {
                        hBrush = CreateSolidBrush(editorBg);
                        tvcd->clrText = editorFg;
                        tvcd->clrTextBk = editorBg;
                    }
                    ++hoverItemIndex;
                    FillRect(tvcd->nmcd.hdc, &rc, hBrush);
                    DeleteObject(hBrush);

                    SetBkMode(tvcd->nmcd.hdc, TRANSPARENT);

                    return CDRF_SKIPDEFAULT;
                }
				} break;
            }
            } // switch
        }
        if (nmhdr->code == TVN_ITEMEXPANDED) {
			if (!commonData.isNppReady) return TRUE;  // Skip processing if Notepad++ isn't ready yet

            LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
            HTREEITEM hItem = pnmtv->itemNew.hItem;
            TVITEM item = { 0 };
            item.mask = TVIF_PARAM;
            item.hItem = hItem;
            if (TreeView_GetItem(hTree, &item)) {
                optional<VFolder*> vFolderOpt = commonData.rootVFolder.findFolderByOrder((int)item.lParam);
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

            std::wstring fontIncreaseLabel = commonData.translator->getTextW("IDM_FONT_INCREASE") + std::to_wstring(commonData.fontSize) + L" px";
            ModifyMenuW(fontSizeSubMenu, 0, MF_BYPOSITION | MF_STRING, MENU_ID_INCREASEFONT, (LPCWSTR)(fontIncreaseLabel.c_str()));

            std::wstring fontDecreaseLabel = commonData.translator->getTextW("IDM_FONT_DECREASE") + std::to_wstring(commonData.fontSize) + L" px";
            ModifyMenuW(fontSizeSubMenu, 1, MF_BYPOSITION | MF_STRING, MENU_ID_DECREASEFONT, (LPCWSTR)(fontDecreaseLabel.c_str()));


            // Show context menu
            TrackPopupMenu(hContextMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndDlg, NULL);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND: {
        HTREEITEM selectedTreeItem = TreeView_GetSelection(hTree);
        if (!selectedTreeItem) {
            break;
        }
        if (LOWORD(wParam) == MENU_ID_INCREASEFONT) {
			increaseFontSize();
            break;
        }
        else if (LOWORD(wParam) == MENU_ID_DECREASEFONT) {
			decreaseFontSize();
			break;
        }
        else if (LOWORD(wParam) == MENU_ID_TREE_DELETE) {
            //TreeView_DeleteItem(hTree, selectedTreeItem);
            return TRUE;
        }
        else if (LOWORD(wParam) == MENU_ID_FILE_CLOSE) {
			TVITEM item = getTreeItem(hTree, selectedTreeItem);
			optional<VFile*> vFileOpt = commonData.rootVFolder.findFileByOrder((int)item.lParam);
            if (vFileOpt) {
                npp(NPPM_MENUCOMMAND, vFileOpt.value()->bufferID, IDM_FILE_CLOSE);
            }
            return TRUE;
        } 
        else if (LOWORD(wParam) == MENU_ID_FILE_WRAP_IN_FOLDER) {
            wrapFileInFolder(selectedTreeItem);
            return TRUE;
        }
        else if (LOWORD(wParam) == MENU_ID_FOLDER_RENAME) {
            TVITEM tvItem = getTreeItem(hTree, selectedTreeItem);
            optional<VFolder*> vFolderOpt = commonData.rootVFolder.findFolderByOrder((int)tvItem.lParam);
            if (!vFolderOpt) {
				return TRUE;
            }
            VFolder* vFolder = vFolderOpt.value();
            
            // Show rename dialog for the folder
            showRenameDialog(vFolder, selectedTreeItem, hTree);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDM_FILE_RENAME)
        {
            nppMenuCall(selectedTreeItem, IDM_FILE_RENAME);
            return TRUE;
        }
        else if (LOWORD(wParam) == MENU_ID_FOLDER_UNWRAP) {
            unwrapFolder(selectedTreeItem);
			return TRUE;
        }
        else if (LOWORD(wParam) == IDM_FILE_SAVE) {
            nppMenuCall(selectedTreeItem, IDM_FILE_SAVE);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDM_FILE_SAVEAS) {
            nppMenuCall(selectedTreeItem, IDM_FILE_SAVEAS);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDM_FILE_OPEN_FOLDER)
        {
            nppMenuCall(selectedTreeItem, IDM_FILE_OPEN_FOLDER);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDM_FILE_OPEN_CMD)
        {
            nppMenuCall(selectedTreeItem, IDM_FILE_OPEN_CMD);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDM_FILE_CONTAININGFOLDERASWORKSPACE)
        {
            nppMenuCall(selectedTreeItem, IDM_FILE_CONTAININGFOLDERASWORKSPACE);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDM_FILE_OPEN_DEFAULT_VIEWER)
        {
            nppMenuCall(selectedTreeItem, IDM_FILE_OPEN_DEFAULT_VIEWER);
            return TRUE;
		}
        else if (LOWORD(wParam) == IDM_FILE_DELETE)
        {
            nppMenuCall(selectedTreeItem, IDM_FILE_DELETE);
			return TRUE;
        }
        else if (LOWORD(wParam) == IDM_FILE_RELOAD)
        {
            nppMenuCall(selectedTreeItem, IDM_FILE_RELOAD);
			return TRUE;
        }
        else if (LOWORD(wParam) == IDM_FILE_PRINT)
        {
            nppMenuCall(selectedTreeItem, IDM_FILE_PRINT);
			return TRUE;
        }
        else if (LOWORD(wParam) == IDM_EDIT_TOGGLEREADONLY)
        {
            LRESULT result = nppMenuCall(selectedTreeItem, IDM_EDIT_TOGGLEREADONLY);
            // TODO: consider view
            if (result) {
                TVITEM item = getTreeItem(hTree, selectedTreeItem);
                optional<VFile*> vFileOpt = commonData.rootVFolder.findFileByOrder((int)item.lParam);
                vFileOpt.value()->isReadOnly = !vFileOpt.value()->isReadOnly;
                changeTreeItemIcon(vFileOpt.value()->bufferID, vFileOpt.value()->view);


                vFileOpt = commonData.rootVFolder.findFileByBufferID(vFileOpt.value()->bufferID, vFileOpt.value()->view == 0 ? 1 : 0); // If this bufferID exist in the other view
                if (vFileOpt) {
                    vFileOpt.value()->isReadOnly = !vFileOpt.value()->isReadOnly;
                    changeTreeItemIcon(vFileOpt.value()->bufferID, vFileOpt.value()->view);
                }
            }
			return TRUE;
        }
        else if (LOWORD(wParam) == IDM_EDIT_FULLPATHTOCLIP)
        {
			nppMenuCall(selectedTreeItem, IDM_EDIT_FULLPATHTOCLIP);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDM_EDIT_FILENAMETOCLIP)
        {
            nppMenuCall(selectedTreeItem, IDM_EDIT_FILENAMETOCLIP);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDM_EDIT_CURRENTDIRTOCLIP)
        {
			nppMenuCall(selectedTreeItem, IDM_EDIT_CURRENTDIRTOCLIP);
			return TRUE;
        }
        else if (LOWORD(wParam) == IDM_VIEW_GOTO_ANOTHER_VIEW)
        {
            TVITEM item = getTreeItem(commonData.hTree, selectedTreeItem);
            optional<VFile*> vFileOpt = commonData.rootVFolder.findFileByOrder((int)item.lParam);
            if (!vFileOpt) {
                return false;
            }

            auto position = npp(NPPM_GETPOSFROMBUFFERID, vFileOpt.value()->bufferID, vFileOpt.value()->view);
            int docView = (position >> 30) & 0x3;   // 0 = MAIN_VIEW, 1 = SUB_VIEW
            int docIndex = position & 0x3FFFFFFF;    // 0-based index

            npp(NPPM_ACTIVATEDOC, vFileOpt.value()->view, docIndex);

            nppMenuCall(selectedTreeItem, IDM_VIEW_GOTO_ANOTHER_VIEW);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDM_VIEW_CLONE_TO_ANOTHER_VIEW)
        {
            nppMenuCall(selectedTreeItem, IDM_VIEW_CLONE_TO_ANOTHER_VIEW);
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
                //COLORREF lineColor = isDarkMode ? RGB(255, 255, 255) : RGB(0, 0, 255);  // White line in dark mode, blue in light mode
				COLORREF lineColor = TreeView_GetTextColor(hTree); // Use text color for better visibility
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
                if (dropItem.iImage == iconIndex[ICON_FOLDER] && !insertion) {
                    // Move file into folder
                    int dragOrder = getOrderFromTreeItem(hTree, hDragItem);
                    int targetOrder = getOrderFromTreeItem(hTree, hDropTarget);
                    auto draggedFileOpt = commonData.rootVFolder.findFileByOrder(dragOrder);
                    auto draggedFolderOpt = commonData.rootVFolder.findFolderByOrder(dragOrder);
                    auto targetFolderOpt = commonData.rootVFolder.findFolderByOrder(targetOrder);

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

                    // Find the dragged item in rootVFolder
                    optional<VFile*> draggedFileOpt = commonData.rootVFolder.findFileByOrder(dragOrder);
                    optional<VFolder*> draggedFolderOpt = commonData.rootVFolder.findFolderByOrder(dragOrder);

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
                commonData.rootVFolder.vFolderSort();
                if (checkRootVFolderJSON()) {
                    LOG("Root is corrupted!!!!!!!!!!!!");
                    checkRootVFolderJSON();
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

    //return CallWindowProc(oldTreeProc, hwndDlg, uMsg, wParam, lParam);
    return FALSE;
}


LRESULT nppMenuCall(HTREEITEM selectedTreeItem, int MENU_ID)
{
    TVITEM item = getTreeItem(commonData.hTree, selectedTreeItem);
    optional<VFile*> vFileOpt = commonData.rootVFolder.findFileByOrder((int)item.lParam);
    if (!vFileOpt) {
        return false;
    }

    if (vFileOpt.value()->bufferID == -1) {
        return false;
	}
    return npp(NPPM_MENUCOMMAND, vFileOpt.value()->bufferID, MENU_ID);

}

void unwrapFolder(HTREEITEM selectedTreeItem)
{
    HWND hTree = GetDlgItem(virtualPanelWnd, IDC_TREE1);
    TVITEM tvItem = getTreeItem(hTree, selectedTreeItem);
    optional<VFolder*> vFolderOpt = commonData.rootVFolder.findFolderByOrder((int)tvItem.lParam);
    if (!vFolderOpt) {
        return;
    }
    VFolder* vFolder = vFolderOpt.value();
	VFolder folderCopy = *vFolder;
    HTREEITEM folderItemToDelete = folderCopy.hTreeItem;
    VFolder* parentFolder = commonData.rootVFolder.findParentFolder(vFolder->getOrder());
	int parentFolderOrder = parentFolder ? parentFolder->getOrder() : -1;

    size_t pos = folderCopy.getOrder();
    HTREEITEM fileTreeItem = nullptr;
    optional<VBase*> aboveSibling = parentFolder ? parentFolder->findAboveSibling(pos) : commonData.rootVFolder.findAboveSibling(pos);
    if (aboveSibling) {
        fileTreeItem = aboveSibling.value()->hTreeItem;
    }
    else {
        fileTreeItem = TVI_FIRST;
    }

    TreeView_DeleteItem(hTree, folderItemToDelete);
    adjustGlobalOrdersForFileMove(folderCopy.getOrder(), commonData.rootVFolder.getLastOrder() + 1);


    BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);
    folderCopy.move(-1);
    vector<VBase*> allChildren = folderCopy.getAllChildren();
    for (const auto& child : allChildren) {
        if (!child) continue;
        if (auto file = dynamic_cast<VFile*>(child)) {
            fileTreeItem = addFileToTree(file, hTree, parentFolder ? parentFolder->hTreeItem : nullptr, isDarkMode, fileTreeItem);
            pos++;
        }
        else if (auto folder = dynamic_cast<VFolder*>(child)) {
            fileTreeItem = addFolderToTree(folder, hTree, parentFolder ? parentFolder->hTreeItem : nullptr, pos, fileTreeItem);
        }
    }

    
    // DONT FORGET:   tree operations first. vData organisations later.
    if (parentFolder && parentFolder->getOrder() != -1) {
        parentFolder->removeChild(vFolder->getOrder());
		parentFolder = commonData.rootVFolder.findFolderByOrder(parentFolderOrder).value();
        auto children = folderCopy.getAllChildren();
        parentFolder->addChildren(children);
    }
    else {
        commonData.rootVFolder.removeChild(vFolder->getOrder());
        auto children = folderCopy.getAllChildren();
        commonData.rootVFolder.addChildren(children);
	}
    
    

    
}

void wrapFileInFolder(HTREEITEM selectedTreeItem)
{
    HWND hTree = GetDlgItem(virtualPanelWnd, IDC_TREE1);
    TVITEM item = getTreeItem(hTree, selectedTreeItem);
    optional<VFile*> vFileOpt = commonData.rootVFolder.findFileByOrder((int)item.lParam);
    if (!vFileOpt) {
        return;
    }

    VFile* vFile = vFileOpt.value();
    VFile fileCopy = *vFile; // Create a copy of VFile*

    int oldOrder = vFile->getOrder();
    int lastOrder = commonData.rootVFolder.getLastOrder();

    VFolder* parentFolder = commonData.rootVFolder.findParentFolder(vFile->getOrder());
    if (parentFolder) {
        parentFolder->removeFile(vFile->getOrder());
    }
    else {
        commonData.rootVFolder.removeFile(vFile->getOrder());
    }
    TreeView_DeleteItem(hTree, selectedTreeItem);
    commonData.rootVFolder.adjustOrders(oldOrder, INT_MAX, 1);


    // Create new folder
    VFolder newFolder;
    newFolder.name = commonData.translator->getText("NEW_FOLDER");
    newFolder.setOrder(oldOrder);
    if (parentFolder) {
        parentFolder->folderList.push_back(newFolder);
    }
    else {
        commonData.rootVFolder.folderList.push_back(newFolder);
    }
    optional<VFolder*> newFolderOpt = commonData.rootVFolder.findFolderByOrder(oldOrder);

    newFolderOpt.value()->addFile(&fileCopy);
    newFolderOpt.value()->isExpanded = true;
    size_t pos = oldOrder;

    /*HTREEITEM folderTreeItem = addFolderToTree(newFolderOpt.value(), hTree, parentFolder ? parentFolder->hTreeItem : nullptr, pos, TVI_LAST);
    TreeView_DeleteItem(hTree, folderTreeItem);*/
    HTREEITEM folderTreeItem = nullptr;
    pos = oldOrder;
    optional<VBase*> aboveSiblingOpt = parentFolder ? parentFolder->findAboveSibling(oldOrder) : commonData.rootVFolder.findAboveSibling(oldOrder);
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
    HWND hTree = GetDlgItem(virtualPanelWnd, IDC_TREE1);
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
    optional<VFile*> selectedFileOpt = commonData.rootVFolder.findFileByOrder(order);

    // If file found and has a valid path, open it
    if (selectedFileOpt && !selectedFileOpt.value()->path.empty()) {
		VFile* selectedFile = selectedFileOpt.value();

        std::wstring wideName(selectedFile->name.begin(), selectedFile->name.end()); // opens by name. not path. With path it opens as a new file
        std::wstring widePath = std::wstring(selectedFile->path.begin(), selectedFile->path.end()); // opens by path.
        if (selectedFile->backupFilePath.empty()) {
            wideName = std::wstring(selectedFile->path.begin(), selectedFile->path.end()); // opens by path.
        }

        LOG("Opening file: [{}]", selectedFile->path);

        // First, try to switch to the file if it's already open
        //ignoreSelectionChange = true;

        intptr_t docOrder = SendMessage(plugin.nppData._nppHandle, NPPM_GETPOSFROMBUFFERID, (WPARAM)selectedFile->bufferID, (LPARAM)selectedFile->view);
        if (docOrder == -1) {
            LOG("docOrder is -1");
            // TODO: bulamadysa ne yapmali
            // OPTION 1: silent remove of tree item and vFile
            return;
        }

        int docView = (docOrder >> 30) & 0x3;   // 0 = MAIN_VIEW, 1 = SUB_VIEW
        int docIndex = docOrder & 0x3FFFFFFF;    // 0-based index

        selectedFile->view = docView;


        npp(NPPM_ACTIVATEDOC, selectedFile->view, docIndex);
        currentView = selectedFile->view;

        // move focus to the right editor
        ::PostMessage(plugin.currentScintilla(), WM_SETFOCUS, 0, 0); // timing issue



        checkReadOnlyStatus(selectedFile);
    }
    else if (selectedFileOpt && selectedFileOpt.value()->path.empty()) {
        OutputDebugStringA("File has empty path, cannot open");
    }
    else {
        OutputDebugStringA("File not found in rootVFolder");
    }
}

void checkReadOnlyStatus(VFile* selectedFile) {
    if (selectedFile == nullptr) return;
    if (!selectedFile->backupFilePath.empty()) return;

    if (selectedFile->view == 0) {
        selectedFile->isReadOnly = SendMessage(plugin.nppData._scintillaMainHandle, SCI_GETREADONLY, 0, 0);
    }
    else {
        selectedFile->isReadOnly = SendMessage(plugin.nppData._scintillaSecondHandle, SCI_GETREADONLY, 0, 0);
    }

    changeTreeItemIcon(selectedFile->bufferID, selectedFile->view);
}

void moveFileIntoFolder(int dragOrder, int targetOrder) {
    auto draggedFileOpt = commonData.rootVFolder.findFileByOrder(dragOrder);
    auto targetFolderOpt = commonData.rootVFolder.findFolderByOrder(targetOrder);
    VFile* file = draggedFileOpt.value();
    VFolder* folder = targetFolderOpt.value();
    VFolder* parentFolder = commonData.rootVFolder.findParentFolder(file->getOrder());
    VFile fileCopy = *file; // Create a copy of VFile*


    HWND hTree = GetDlgItem(virtualPanelWnd, IDC_TREE1);

    // Remove dragged item from tree
    TreeView_DeleteItem(hTree, hDragItem);
    // Add dragged item as child of target folder in tree
    addFileToTree(&fileCopy, hTree, hDropTarget, npp(NPPM_ISDARKMODEENABLED, 0, 0), TVI_LAST);


    if (parentFolder) {
        parentFolder->removeFile(file->getOrder());
    }
    else {
        commonData.rootVFolder.removeFile(file->getOrder());
    }
    commonData.rootVFolder.adjustOrders(fileCopy.getOrder(), INT_MAX, -1);
    // 
    folder->addFile(&fileCopy);
    int tempOrder = fileCopy.getOrder();
    commonData.rootVFolder.adjustOrders(fileCopy.getOrder(), INT_MAX, 1);
    // Update the file's order to match the folder's order
    file = commonData.rootVFolder.findFileByPath(fileCopy.path);
    file->setOrder(tempOrder);

    writeJsonFile();

    LOG("[{}] moved into folder [{}]'s end", file->name, folder->name);
}

void moveFolderIntoFolder(int dragOrder, int targetOrder) {
    // TODO: folder into folder
    auto draggedFolderOpt = commonData.rootVFolder.findFolderByOrder(dragOrder);
    auto targetFolderOpt = commonData.rootVFolder.findFolderByOrder(targetOrder);

    VFolder* movedFolder = draggedFolderOpt.value();
    VFolder* targetFolder = targetFolderOpt.value();

    int step = targetFolder->getLastOrder() - dragOrder + 1;
    if (dragOrder < targetOrder) {
        step = step - movedFolder->countItemsInFolder();
    }
    VFolder movedFolderCopy = *movedFolder;
	
    
    optional<VFolder*> sourceParentFolder = commonData.rootVFolder.findParentFolder(dragOrder);
    if (sourceParentFolder.value() != nullptr) {
        sourceParentFolder.value()->removeChild(dragOrder);
    }
    else {
		commonData.rootVFolder.removeChild(dragOrder);
    }
    targetFolderOpt = commonData.rootVFolder.findFolderByOrder(targetOrder); // removeChild operation somehow effected targetFolder
    targetFolder = targetFolderOpt.value();

    size_t pos = targetFolder->getLastOrder();
	adjustGlobalOrdersForFolderMove(movedFolderCopy.getOrder(), pos + 1, movedFolderCopy.countItemsInFolder());
    movedFolderCopy.move(step);
    targetFolder->folderList.push_back(movedFolderCopy);
    movedFolder = commonData.rootVFolder.findFolderByOrder(movedFolderCopy.getOrder()).value();
    
	pos = movedFolder->getOrder();
    HWND hTree = GetDlgItem(virtualPanelWnd, IDC_TREE1);
    
    HTREEITEM folderTreeItem = addFolderToTree(movedFolder, hTree, targetFolder->hTreeItem, pos, TVI_LAST);

	TreeView_DeleteItem(hTree, hDragItem);
}

HTREEITEM addFileToTree(VFile* vFile, HWND hTree, HTREEITEM hParent, bool darkMode, HTREEITEM hPrevItem) {
    wchar_t buffer[100];

    if (vFile->name.find("\\") != std::string::npos) {
		LOG("Invalid file name: [{}]", vFile->name);
    }

    TVINSERTSTRUCT tvis = { 0 };
    tvis.hParent = hParent;
    tvis.hInsertAfter = hPrevItem;
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_STATE;
    wcscpy_s(buffer, 100, toWchar(vFile->name));
    tvis.item.pszText = buffer;

    
    
    if (vFile->view == 1) {
        tvis.item.stateMask = TVIS_STATEIMAGEMASK;
        tvis.item.state = INDEXTOSTATEIMAGEMASK(ICON_FILE_SECONDARY_VIEW); // 1-based index in state image list
    }

    if (vFile->isReadOnly) {
        if (darkMode) {
            tvis.item.iImage = iconIndex[ICON_FILE_READONLY_DARK]; // Use read-only icon
            tvis.item.iSelectedImage = iconIndex[ICON_FILE_READONLY_DARK]; // Use read-only icon        
        }
        else {
            tvis.item.iImage = iconIndex[ICON_FILE_READONLY_LIGHT]; // Use read-only icon
            tvis.item.iSelectedImage = iconIndex[ICON_FILE_READONLY_LIGHT]; // Use read-only icon
        }
    }
	else if (vFile->isEdited) {
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

    /*int lastOrder = vFolder->fileList.empty() ? 0 : vFolder->fileList.back().getOrder();
    lastOrder = std::max(lastOrder, vFolder->folderList.empty() ? 0 : vFolder->folderList.back().getOrder());*/

	int lastOrder = vFolder->getLastOrder();
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
            else {
                // This should not happen, but if it does, we skip the order. 
                pos++;
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
    if (1 == 1) return;
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
    vector<VFile*> allFiles = commonData.rootVFolder.getAllFiles();
    for (VFile* file : allFiles) {
        changeTreeItemIcon(file->bufferID, file->view);
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
    FileLocation sourceLocation = findFileLocation(oldOrder);
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
    int rootItemCount = commonData.rootVFolder.fileList.size() + commonData.rootVFolder.folderList.size(); // TODO: add a class function if the order is in root
    
    if (commonData.rootVFolder.isInRoot(newOrder)) {
        movingToRoot = true;
        targetFolder = nullptr;
    } else {
        FileLocation targetLocation = findFileLocation(newOrder);
		targetFolder = targetLocation.parentFolder;
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
        commonData.rootVFolder.fileList.push_back(fileData);
        movedFile = commonData.rootVFolder.findFileByOrder(newOrder).value();

        HWND hTree = GetDlgItem(virtualPanelWnd, IDC_TREE1);
        HTREEITEM oldItem = fileData.hTreeItem;
        HTREEITEM prevItem = nullptr;
        optional<VBase*> aboveSibling = commonData.rootVFolder.findAboveSibling(newOrder);
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
        if (targetFolder == nullptr) {
            // TODO: exception
            return;
        }
        // Moving from root to folder
        // Store the file data before removing
        VFile fileData = *movedFile;
        
        // First, adjust global orders to make space for the move
        adjustGlobalOrdersForFileMove(oldOrder, newOrder);
        
        // Remove from root
        auto& rootFileList = commonData.rootVFolder.fileList;
        rootFileList.erase(std::remove_if(rootFileList.begin(), rootFileList.end(),
            [movedFile](const VFile& file) { return &file == movedFile; }), rootFileList.end());

		if (newOrder > oldOrder) newOrder--;
        // Find the target folder (simplified - would need proper logic)
        // For now, we'll assume the first folder
        if (!commonData.rootVFolder.folderList.empty()) {
            // Set new order and add to folder
			fileData.setOrder(newOrder);
            targetFolder->fileList.push_back(fileData);
            movedFile = targetFolder->findFileByOrder(newOrder).value();
        }

        HWND hTree = GetDlgItem(virtualPanelWnd, IDC_TREE1);
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
        HWND hTree = GetDlgItem(virtualPanelWnd, IDC_TREE1);
        HTREEITEM oldItem = nullptr;
        HTREEITEM hParent = nullptr;
		HTREEITEM prevItem = nullptr;

        int firstOrderOfFolder = 0; // first order of root
        if (sourceFolder) {
			//hParent = FindItemByLParam(hTree, nullptr, (LPARAM)sourceFolder->getOrder());
			firstOrderOfFolder = sourceFolder->getOrder() + 1;
        }
        
		// What if newOrder is the first item in the folder?
        oldItem = FindItemByLParam(hTree, hParent, (LPARAM)movedFile->getOrder());
		hParent = sourceFolder ? sourceFolder->hTreeItem : nullptr;
        if (newOrder == firstOrderOfFolder) {
			prevItem = TVI_FIRST;
        } else {
            optional<VBase*> targetBelowSibling;
            if (newOrder < oldOrder) {
                targetBelowSibling = commonData.rootVFolder.getChildByOrder(newOrder);
            }
            else {
                targetBelowSibling = commonData.rootVFolder.getChildByOrder(newOrder + 1);
            }
            HTREEITEM targetNextItem = targetBelowSibling ? targetBelowSibling.value()->hTreeItem : nullptr;
            prevItem = TreeView_GetPrevSibling(hTree, targetNextItem);

		}
        TreeView_DeleteItem(hTree, oldItem);

        adjustGlobalOrdersForFileMove(oldOrder, newOrder > oldOrder ? newOrder+1 : newOrder);
		movedFile->setOrder(newOrder);

		addFileToTree(movedFile, hTree, hParent, isDarkMode, prevItem);

        LOG("[{}] moved to order [{}]", movedFile->name, newOrder);

    }
}

void reorderFolders(int oldOrder, int newOrder) {
    // Find the folder to move using recursive search
    FolderLocation moveLocation = findFolderLocation(oldOrder);

    if (!moveLocation.found) {
        return;  // Folder not found
    }

    HWND hTree = GetDlgItem(virtualPanelWnd, IDC_TREE1);
    optional<VFolder*> movedFolderOpt = commonData.rootVFolder.findFolderByOrder(oldOrder);
	VFolder* movedFolder = movedFolderOpt.value();
    VFolder* sourceParentFolder = moveLocation.parentFolder;
    VFolder* targetParentFolder = commonData.rootVFolder.findParentFolder(newOrder);


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
        int firstFileOrder = -1, firstFolderOrder = -1;
        if (!commonData.rootVFolder.fileList.empty()) {
            firstFileOrder = commonData.rootVFolder.fileList[0].getOrder();
        }
        if (!commonData.rootVFolder.folderList.empty()) {
            firstFolderOrder = commonData.rootVFolder.folderList[0].getOrder();
        }
        firstOrderOfFolder = min(firstFileOrder, firstFolderOrder);
    }

    if (newOrder == firstOrderOfFolder) {
        prevItem = TVI_FIRST;
    } else {
        // Find the item that should come before the moved folder
        VFolder* parentFolder = commonData.rootVFolder.findParentFolder(newOrder);
        optional<VBase*> aboveSibling = parentFolder ? parentFolder->findAboveSibling(newOrder) : commonData.rootVFolder.findAboveSibling(newOrder);
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
        commonData.rootVFolder.removeChild(oldOrder);
	}

    targetParentFolder = commonData.rootVFolder.findParentFolder(newOrder);
    targetParentItem = targetParentFolder ? targetParentFolder->hTreeItem : nullptr;

    if (targetParentFolder) {
        targetParentFolder->folderList.push_back(movedFolderCopy);
    }
    else {
        commonData.rootVFolder.folderList.push_back(movedFolderCopy);
	}
	movedFolder = commonData.rootVFolder.findFolderByOrder(movedFolderCopy.getOrder()).value();

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
    commonData.rootVFolder.vFolderSort();
}

FileLocation findFileLocation(int order) {
    FileLocation location;
    // TODO: change the folder location according to insertion mark 


    // Check root level first
    for (auto& file : commonData.rootVFolder.fileList) {
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
            if (subFolder.getOrder() == order) {
                location.file = nullptr;
				location.parentFolder = &folder;
				location.found = true;
                return true;
			}
            if (searchInFolder(subFolder)) {
                return true;
            }
        }
        return false;
    };

    // Search in all root-level folders
    for (auto& rootFolder : commonData.rootVFolder.folderList) {
        if (searchInFolder(rootFolder)) {
            break;
        }
    }

    return location;
}

void changeTreeItemIcon(UINT_PTR bufferID, int view) 
{
    BOOL isDarkMode = npp(NPPM_ISDARKMODEENABLED, 0, 0);
	optional<VFile*> vFileOpt = commonData.rootVFolder.findFileByBufferID(bufferID, view);
    if (!vFileOpt) {
        return;
	}

    TVITEM item = { 0 };
    item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_STATE;
    item.hItem = vFileOpt.value()->hTreeItem;

    wchar_t* name = toWchar(vFileOpt.value()->name);
    item.pszText = name;


    VFile* vFile = vFileOpt.value();

    

    if (vFile->view == 1) {
        item.stateMask = TVIS_STATEIMAGEMASK;
        item.state = INDEXTOSTATEIMAGEMASK(ICON_FILE_SECONDARY_VIEW); // 1-based index in state image list
    }
    else {
        item.stateMask = TVIS_STATEIMAGEMASK;
        item.state = 0;   // no state image
    }

    if (vFile->isReadOnly && isDarkMode) 
    {
        item.iImage = iconIndex[ICON_FILE_READONLY_DARK]; // Use read-only icon
        item.iSelectedImage = iconIndex[ICON_FILE_READONLY_DARK]; // Use read-only icon

        TreeView_SetItem(commonData.hTree, &item);
        return;
    } else if (vFile->isReadOnly && !isDarkMode) 
    {
        item.iImage = iconIndex[ICON_FILE_READONLY_LIGHT]; // Use read-only icon
        item.iSelectedImage = iconIndex[ICON_FILE_READONLY_LIGHT]; // Use read-only icon
    
        TreeView_SetItem(commonData.hTree, &item);
        return;
	}

	if (vFile->isEdited || (commonData.bufferStates.find(bufferID) != commonData.bufferStates.end() && !commonData.bufferStates[bufferID])) {
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

void activateSibling(bool aboveSibling) 
{
    UINT_PTR bufferID = ::SendMessage(plugin.nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0);
    optional<VFile*> vFileOpt = commonData.rootVFolder.findFileByBufferID(bufferID, currentView);
    if (!vFileOpt) {
        return;
	}
	VFile* vFile = vFileOpt.value();
	int currentOrder = vFile->getOrder();
    do {
        if (aboveSibling) {
            currentOrder--;
        }
        else {
            currentOrder++;
        }

	    optional<VBase*> sibling = commonData.rootVFolder.getChildByOrder(currentOrder);
        if (!sibling) {
            if (aboveSibling) {
                currentOrder = commonData.rootVFolder.getLastOrder() + 1;
            }
            else {
				currentOrder = -1;
            }
            continue;
		}
        if (auto file = dynamic_cast<VFile*>(sibling.value())) {
			treeItemSelected(file->hTreeItem);
			return;
        }
		currentOrder = sibling.value()->getOrder();
    } while (true);
}

extern void increaseFontSize() {
    if (!commonData.hTree) return;
    commonData.fontSize++;
	setFontSize();
}

extern void decreaseFontSize() {
    if (!commonData.hTree) return;
    if (commonData.fontSize > 6) {
        commonData.fontSize--;
    }
	setFontSize();
}

extern void setFontSize() {
    if (!commonData.hTree) return;
    
    LOGFONTW lf = {};
    SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(lf), &lf, 0);

    // adjust font size (lfHeight is in *logical units* — negative means character height in pixels)
    //lf.lfHeight = -commonData.fontSize;  // e.g. -14 for 14px font
    lf.lfHeight = -MulDiv(commonData.fontSize, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72);
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, commonData.fontFamily.value.c_str());

    HFONT hFont = CreateFontIndirectW(&lf);
    SendMessage(commonData.hTree, WM_SETFONT, (WPARAM)hFont, TRUE);



    // adjust item height to match new font
    HDC hdc = GetDC(commonData.hTree);
    TEXTMETRIC tm;
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);
    GetTextMetrics(hdc, &tm);
    SelectObject(hdc, hOld);
    ReleaseDC(commonData.hTree, hdc);

    int newHeight = tm.tmHeight + 4;
	if (newHeight < 21) { // magic number. this is because icon sizes
        newHeight = 21;
    }

    SendMessage(commonData.hTree, TVM_SETITEMHEIGHT, (WPARAM)newHeight, 0);



    // update menu text
    wstring fontIncreaseLabel = commonData.translator->getTextW("IDM_FONT_INCREASE") + to_wstring(commonData.fontSize) + L" px";
    wstring fontDecreaseLabel = commonData.translator->getTextW("IDM_FONT_DECREASE") + to_wstring(commonData.fontSize) + L" px";

    wchar_t buffer[256];
    HMENU pluginMenu;
    HMENU hMenu = (HMENU)npp(NPPM_GETMENUHANDLE, (WPARAM)NPPPLUGINMENU, (LPARAM)0);

    int pluginMenuIndex = 0;
	int pluginMenuSize = GetMenuItemCount(hMenu);
    for (pluginMenuIndex = 0; pluginMenuIndex < pluginMenuSize; pluginMenuIndex++) {
        int copied = GetMenuStringW(hMenu, pluginMenuIndex, buffer, _countof(buffer), MF_BYPOSITION);
		if (fromWchar(buffer) == "VirtualFolders") {  // TODO: change to dynamic plugin name ?????
            break;
        }
    }
    if (pluginMenuIndex == pluginMenuSize) return;

    pluginMenu = GetSubMenu(hMenu, pluginMenuIndex);
    if (pluginMenu) {
        ModifyMenuW(pluginMenu, menuItem_IncreaseFont, MF_BYPOSITION | MF_STRING, menuDefinition[menuItem_IncreaseFont]._cmdID, (LPCWSTR)(fontIncreaseLabel.c_str()));
        ModifyMenuW(pluginMenu, menuItem_DecreaseFont, MF_BYPOSITION | MF_STRING, menuDefinition[menuItem_DecreaseFont]._cmdID, (LPCWSTR)(fontDecreaseLabel.c_str()));
    }
}
