#pragma once


#include <windows.h>



inline ULONGLONG filetimeToInteger(FILETIME fileTime) {
	ULARGE_INTEGER u;
    u.LowPart = fileTime.dwLowDateTime;
	u.HighPart = fileTime.dwHighDateTime;
	return u.QuadPart;
}

inline FILETIME ULongLongToFileTime(ULONGLONG filetime) {
    FILETIME ft;
    ft.dwLowDateTime  = static_cast<DWORD>(filetime & 0xFFFFFFFF);
    ft.dwHighDateTime = static_cast<DWORD>((filetime >> 32) & 0xFFFFFFFF);
    return ft;
}