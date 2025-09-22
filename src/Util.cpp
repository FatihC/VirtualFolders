#pragma once

#include "Util.h"

using namespace std;




wchar_t* toWchar(const string& str) {
    static wchar_t buffer[256];
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer, 256);
    return buffer;
}

string fromWchar(const wchar_t* wstr) {
    if (!wstr) return {};

    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (sizeNeeded <= 1) return {}; // empty or error

    string result(sizeNeeded - 1, '\0'); // exclude null terminator
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, result.data(), sizeNeeded, nullptr, nullptr);

    return result;
}


wstring toWstring(const string& str) {
    if (str.empty()) return L"";

    // Get required size (in wchar_t's)
    int size_needed = MultiByteToWideChar(
        CP_UTF8,            // source is UTF-8
        0,                  // no special flags
        str.c_str(),        // input string
        (int)str.size(),    // number of chars to convert
        nullptr,            // don't output yet
        0                   // request size only
    );

    // Allocate std::wstring with proper size
    wstring wstr(size_needed, 0);

    // Do the actual conversion
    MultiByteToWideChar(
        CP_UTF8, 0,
        str.c_str(), (int)str.size(),
        &wstr[0], size_needed
    );

    return wstr;
}