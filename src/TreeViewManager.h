#pragma once

#include "model/VData.h"
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <string>
#include "Util.h"



#define TREEVIEW_TIMER_ID 1




// Forward declarations
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
// Helper function to find which container holds a file with the given order
struct FileLocation {
    VFolder* parentFolder = nullptr;  // nullptr means root level
    VFile* file = nullptr;
    bool found = false;
};

// Helper function to find which container holds a folder with the given order
struct FolderLocation {
    VFolder* parentFolder = nullptr;  // nullptr means root level
    VFolder* folder = nullptr;
    bool found = false;
};


enum IconType {
    ICON_APP,
    ICON_FOLDER,
    ICON_FILE_LIGHT,
    ICON_FILE_DARK,
    ICON_FILE_EDITED,
	ICON_FILE_READONLY_DARK,
	ICON_FILE_READONLY_LIGHT,
    ICON_FILE_SECONDARY_VIEW
};

extern std::unordered_map<IconType, int> iconIndex;


// Menu command IDs
#define MENU_ID_TREE_DELETE 40001
#define MENU_ID_INCREASEFONT 40002
#define MENU_ID_DECREASEFONT 40003
#define MENU_ID_FILE_CLOSE 40100
#define MENU_ID_FILE_WRAP_IN_FOLDER 40101
#define MENU_ID_FOLDER_RENAME 40103
#define MENU_ID_FOLDER_UNWRAP 40104




// Global variables for TreeView management
inline HTREEITEM hDragItem = nullptr;
inline HTREEITEM hDropTarget = nullptr;
inline HIMAGELIST hDragImage = nullptr;
inline bool isDragging = false;
inline bool ignoreSelectionChange = false;
inline bool contextMenuLoaded = false;
inline int currentView = -1;



INT_PTR CALLBACK fileViewDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
// TreeView management functions
void updateTreeItemLParam(VBase* vBase);
HTREEITEM addFileToTree(VFile* vFile, HWND hTree, HTREEITEM hParent, bool darkMode, HTREEITEM hPrevItem);
HTREEITEM addFolderToTree(VFolder* vFolder, HWND hTree, HTREEITEM hParent, size_t& pos, HTREEITEM hPrevItem);
void updateTreeColorsExternal(HWND hTree);

// Drag & Drop and Reordering functions
void reorderItems(int oldOrder, int newOrder);
void reorderFolders(int oldOrder, int newOrder);
void refreshTree(HWND hTree, VData& vData);
void adjustGlobalOrdersForFileMove(int oldOrder, int newOrder);
void adjustOrdersInContainer(vector<VFolder>& folders, vector<VFile>& files, int oldOrder, int newOrder);
void checkReadOnlyStatus(VFile* selectedFile);


// Helper functions
FileLocation findFileLocation(VData& vData, int order);
int getOrderFromTreeItem(HWND hTree, HTREEITEM hItem);
TVITEM getTreeItem(HWND hTree, HTREEITEM hItem);
HTREEITEM FindItemByLParam(HWND hTree, HTREEITEM hParent, LPARAM lParam);


void changeTreeItemIcon(UINT_PTR bufferID, int view);
FolderLocation findFolderLocation(VData& vData, int order);
void adjustGlobalOrdersForFolderMove(int oldOrder, int newOrder, int folderItemCount);

void activateSibling(bool aboveSibling);
extern void increaseFontSize();
extern void decreaseFontSize();
extern void setFontSize();


// Dialog procedure for the file view dialog
// INT_PTR CALLBACK fileViewDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
