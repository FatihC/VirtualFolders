#pragma once

#include <string>
#include <windows.h>
#include <vector>

using namespace std;

using BYTE = unsigned char;


wchar_t* toWchar(const string& str);

string fromWchar(const wchar_t* wstr);

wstring toWstring(const string& str);

string base64_encode(const string& in);
vector<BYTE> base64_decode(const string& in);

vector<BYTE> safeCompress(const vector<BYTE>& input);

vector<BYTE> safeDecompress(const vector<BYTE>& input, SIZE_T originalSize);
std::vector<BYTE> safeDecompress(const std::vector<BYTE>& compressed, size_t originalSize = 0);

wstring UrlEncode(const wstring& value);

wstring GetAppVersion();
wstring GetPluginVersion(HMODULE hModule);