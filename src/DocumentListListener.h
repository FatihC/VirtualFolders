// DocumentListListener.h
#pragma once
#include <windows.h>


#define DOC_LIST_HOOK_TIMER 1



bool hookDocList(HWND nppHandle);
void unhookDocList();
