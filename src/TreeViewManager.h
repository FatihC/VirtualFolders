#pragma once

#include "model/VData.h"
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <string>

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
    ICON_FOLDER,
    ICON_FILE,
    ICON_FILE_LIGHT,
    ICON_FILE_DARK,
    ICON_FILE_EDITED
};

extern std::unordered_map<IconType, int> iconIndex;
extern std::string new_line;


// Menu command IDs
#define ID_TREE_DELETE 40001
#define ID_FILE_CLOSE 40100
#define ID_FILE_WRAP_IN_FOLDER 40101
#define ID_FILE_RENAME 40102
#define ID_FOLDER_RENAME 40103
#define ID_FOLDER_UNWRAP 40104


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

// Helper functions
FileLocation findFileLocation(VData& vData, int order);
int getOrderFromTreeItem(HWND hTree, HTREEITEM hItem);
TVITEM getTreeItem(HWND hTree, HTREEITEM hItem);
HTREEITEM FindItemByLParam(HWND hTree, HTREEITEM hParent, LPARAM lParam);

// Utility functions
wchar_t* toWchar(const std::string& str);
std::wstring toWstring(const std::string& str);
std::string fromWchar(const wchar_t* wstr);

void changeTreeItemIcon(bool isDirty, VFile* vFile);
FolderLocation findFolderLocation(VData& vData, int order);
void adjustGlobalOrdersForFolderMove(int oldOrder, int newOrder, int folderItemCount);

// Dialog procedure for the file view dialog
// INT_PTR CALLBACK fileViewDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
