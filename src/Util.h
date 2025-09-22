#pragma once

#include <string>
#include <windows.h>

using namespace std;




wchar_t* toWchar(const string& str);

string fromWchar(const wchar_t* wstr);

wstring toWstring(const string& str);
