#pragma once

#include <string>
#include <windows.h>
#include <vector>

using namespace std;




wchar_t* toWchar(const string& str);

string fromWchar(const wchar_t* wstr);

wstring toWstring(const string& str);

string base64_encode(const string& in);

vector<BYTE> safeCompress(const vector<BYTE>& input);

vector<BYTE> safeDecompress(const vector<BYTE>& input, SIZE_T originalSize);

wstring UrlEncode(const wstring& value);

wstring GetAppVersion();
wstring GetPluginVersion(HMODULE hModule);