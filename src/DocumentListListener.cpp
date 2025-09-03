#include <vector>
#include <string>
#include <windows.h>
#include <commctrl.h>
#include "DocumentListListener.h"
#include <format>


#define LOG(fmt, ...) { \
    auto msg = std::format(fmt, __VA_ARGS__); \
    msg = "***** VFOLDER LOG ***** : " + msg; \
    msg += "\n"; \
    OutputDebugStringA(msg.c_str()); \
}


HWND g_docListHwnd = nullptr;
WNDPROC g_origDocListProc = nullptr;
std::vector<std::wstring> g_lastOrder;






HWND findDocListHWND(HWND nppHandle) {
    HWND hDockingPanel = FindWindowEx(nppHandle, NULL, L"DockingContainer", NULL);
    while (hDockingPanel) {
        HWND hListView = FindWindowEx(hDockingPanel, NULL, L"SysListView32", NULL);
        if (hListView) return hListView;
        hDockingPanel = FindWindowEx(nppHandle, hDockingPanel, L"DockingContainer", NULL);
    }
    return NULL;
}

HWND FindChildByClass(HWND parent, const wchar_t* className) {
    HWND h = FindWindowEx(parent, NULL, className, NULL);
    if (h) return h;

    for (HWND child = FindWindowEx(parent, NULL, NULL, NULL); child; child = FindWindowEx(parent, child, NULL, NULL)) {
        HWND result = FindChildByClass(child, className);
        if (result) return result;
    }
    return NULL;
}


std::vector<std::wstring> getDocListItems(HWND hList) {
    std::vector<std::wstring> items;
    int count = ListView_GetItemCount(hList);

    for (int i = 0; i < count; ++i) {
        wchar_t buf[MAX_PATH]{};
        ListView_GetItemText(hList, i, 0, buf, MAX_PATH);
        items.push_back(buf);
    }
    return items;
}

bool orderChanged(HWND hList) {
    auto now = getDocListItems(hList);
    if (now != g_lastOrder) {
        g_lastOrder = now;
        return true;
    }
    return false;
}


LRESULT CALLBACK DocListSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NOTIFY) {
        LPNMHDR nmhdr = (LPNMHDR)lParam;

        wchar_t buf[128];
        swprintf(buf, 128, L"nmhdr->code = %u\n", nmhdr->code);
        OutputDebugString(buf);

        if (nmhdr->code == -12) {
		    LOG("Document order changed: wParam: {}, lParam: {}", wParam, lParam);
            if (orderChanged(hwnd)) {
                // Do whatever you want here (raise custom event, update UI, etc.)
            }
        }
    }

    return CallWindowProc(g_origDocListProc, hwnd, msg, wParam, lParam);
}

bool hookDocList(HWND nppHandle) {
    g_docListHwnd = findDocListHWND(nppHandle);
    if (!g_docListHwnd) {
        g_docListHwnd = FindChildByClass(nppHandle, L"SysListView32");
	}
    if (g_docListHwnd) {
        g_lastOrder = getDocListItems(g_docListHwnd);
        g_origDocListProc = (WNDPROC)SetWindowLongPtr(g_docListHwnd, GWLP_WNDPROC, (LONG_PTR)DocListSubclassProc);

        return true;
    }
    return false;
}

void unhookDocList() {
    if (g_docListHwnd && g_origDocListProc) {
        SetWindowLongPtr(g_docListHwnd, GWLP_WNDPROC, (LONG_PTR)g_origDocListProc);
        g_docListHwnd = nullptr;
        g_origDocListProc = nullptr;
    }
}