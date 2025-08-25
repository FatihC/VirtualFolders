#pragma once

#include "model/VData.h"
#include <windows.h>

// Function to show the rename dialog for a VBase item (VFile or VFolder)
void showRenameDialog(VBase* item, HTREEITEM treeItem, HWND hTree);
