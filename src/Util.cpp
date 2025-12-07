#pragma once

#include "Util.h"

#include <windows.h>
#include <compressapi.h>
#include <stdexcept>
#pragma comment(lib, "Cabinet.lib")
#include <sstream>
#include <iomanip>



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

    // Allocate wstring with proper size
    wstring wstr(size_needed, 0);

    // Do the actual conversion
    MultiByteToWideChar(
        CP_UTF8, 0,
        str.c_str(), (int)str.size(),
        &wstr[0], size_needed
    );

    return wstr;
}

string base64_encode(const string& in) {
    static const char* b64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(b64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(b64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

int main() {
	
    return 0;
}

vector<BYTE> base64_decode(const string& in) {
    if (in.empty()) return {};

    // Build decode table
    vector<int> T(256, -1);
    for (int i = 0; i < 26; ++i) { T['A' + i] = i; T['a' + i] = 26 + i; }
    for (int i = 0; i < 10; ++i) T['0' + i] = 52 + i;
    T['+'] = 62; T['/'] = 63;

    // Remove whitespace (if any)
    string s;
    s.reserve(in.size());
    for (char c : in) if (!isspace(static_cast<unsigned char>(c))) s.push_back(c);
    if (s.empty()) return {};

    size_t pads = 0;
    if (!s.empty() && s.back() == '=') { ++pads; if (s.size() > 1 && s[s.size() - 2] == '=') ++pads; }

    size_t groups = s.size() / 4;
    vector<BYTE> out;
    out.reserve(groups * 3);

    for (size_t i = 0; i < groups; ++i) {
        int idx = static_cast<int>(i * 4);
        int v0 = T[static_cast<unsigned char>(s[idx])];
        int v1 = T[static_cast<unsigned char>(s[idx + 1])];
        int v2 = (s[idx + 2] == '=') ? 0 : T[static_cast<unsigned char>(s[idx + 2])];
        int v3 = (s[idx + 3] == '=') ? 0 : T[static_cast<unsigned char>(s[idx + 3])];
        int n = (v0 << 18) | (v1 << 12) | (v2 << 6) | v3;
        out.push_back(static_cast<BYTE>((n >> 16) & 0xFF));
        out.push_back(static_cast<BYTE>((n >> 8) & 0xFF));
        out.push_back(static_cast<BYTE>(n & 0xFF));
    }

    if (pads) out.resize(out.size() - pads);
    return out;
}



typedef BOOL(WINAPI* CreateCompressor_t)(DWORD, PCOMPRESS_ALLOCATION_ROUTINES, PCOMPRESSOR_HANDLE);
typedef BOOL(WINAPI* Compress_t)(COMPRESSOR_HANDLE, LPCVOID, SIZE_T, PVOID, SIZE_T, PSIZE_T);
typedef BOOL(WINAPI* CloseCompressor_t)(COMPRESSOR_HANDLE);


vector<BYTE> safeCompress(const vector<BYTE>& input) {
    HMODULE hCabinet = LoadLibraryW(L"Cabinet.dll");
    if (!hCabinet) {
        // Cabinet.dll not found
        return vector<BYTE>(input.begin(), input.end()); // fallback: no compression
    }

    auto pCreateCompressor = reinterpret_cast<CreateCompressor_t>(
        GetProcAddress(hCabinet, "CreateCompressor"));
    auto pCompress = reinterpret_cast<Compress_t>(
        GetProcAddress(hCabinet, "Compress"));
    auto pCloseCompressor = reinterpret_cast<CloseCompressor_t>(
        GetProcAddress(hCabinet, "CloseCompressor"));

    if (!pCreateCompressor || !pCompress || !pCloseCompressor) {
        // Functions not available (Windows 7 or earlier)
        FreeLibrary(hCabinet);
        return vector<BYTE>(input.begin(), input.end());
    }

    COMPRESSOR_HANDLE compressor = NULL;
    if (!pCreateCompressor(COMPRESS_ALGORITHM_MSZIP, NULL, &compressor)) {
        FreeLibrary(hCabinet);
        throw runtime_error("Failed to create compressor");
    }

    SIZE_T compressedSize = 0;
    pCompress(compressor, input.data(), input.size(), NULL, 0, &compressedSize);

    vector<BYTE> output(compressedSize);
    if (!pCompress(compressor, input.data(), input.size(), output.data(), output.size(), &compressedSize)) {
        pCloseCompressor(compressor);
        FreeLibrary(hCabinet);
        throw runtime_error("Compression failed");
    }

    output.resize(compressedSize);
    pCloseCompressor(compressor);
    FreeLibrary(hCabinet);
    return output;
}

vector<BYTE> safeDecompress(const vector<BYTE>& input, SIZE_T originalSize) {
    HMODULE hCabinet = LoadLibraryW(L"Cabinet.dll");
    if (!hCabinet) {
        // Cabinet.dll not found, fallback: return input
        return input;
    }

    using CreateDecompressor_t = BOOL(WINAPI*)(DWORD, PCOMPRESS_ALLOCATION_ROUTINES, PDECOMPRESSOR_HANDLE);
    using Decompress_t = BOOL(WINAPI*)(DECOMPRESSOR_HANDLE, LPCVOID, SIZE_T, PVOID, SIZE_T, PSIZE_T);
    using CloseDecompressor_t = BOOL(WINAPI*)(DECOMPRESSOR_HANDLE);

    auto pCreateDecompressor = reinterpret_cast<CreateDecompressor_t>(GetProcAddress(hCabinet, "CreateDecompressor"));
    auto pDecompress = reinterpret_cast<Decompress_t>(GetProcAddress(hCabinet, "Decompress"));
    auto pCloseDecompressor = reinterpret_cast<CloseDecompressor_t>(GetProcAddress(hCabinet, "CloseDecompressor"));

    if (!pCreateDecompressor || !pDecompress || !pCloseDecompressor) {
        FreeLibrary(hCabinet);
        return input;
    }

    DECOMPRESSOR_HANDLE decompressor = NULL;
    if (!pCreateDecompressor(COMPRESS_ALGORITHM_MSZIP, nullptr, &decompressor)) {
        FreeLibrary(hCabinet);
        throw runtime_error("CreateDecompressor failed");
    }

    vector<BYTE> output(originalSize);
    SIZE_T decompressedSize = 0;

    originalSize = 0;
    while (true) {
        originalSize++;

        vector<BYTE> output(originalSize);
        SIZE_T decompressedSize = 0;
        if (!pDecompress(decompressor, input.data(), input.size(), output.data(), output.size(), &decompressedSize)) {
            OutputDebugStringA("Decompression attempt failed with output size:" + originalSize);
        } else {
            OutputDebugStringA("Decompression attempt successfull with output size:" + originalSize);
            output.resize(decompressedSize);
            pCloseDecompressor(decompressor);
            FreeLibrary(hCabinet);
			return output;
		}
    }

    if (!pDecompress(decompressor, input.data(), input.size(), output.data(), output.size(), &decompressedSize)) {
        pCloseDecompressor(decompressor);
        FreeLibrary(hCabinet);
        throw runtime_error("Decompression failed");
    }

    output.resize(decompressedSize);
    pCloseDecompressor(decompressor);
    FreeLibrary(hCabinet);
    return output;
}



wstring UrlEncode(const wstring& value)
{
    wostringstream escaped;
    escaped.fill(L'0');
    escaped << hex;

    for (wchar_t c : value)
    {
        if (iswalnum(c) || c == L'-' || c == L'_' || c == L'.' || c == L'~')
        {
            escaped << c;
        }
        else if (c == L' ')
        {
            escaped << L"%20";
        }
        else
        {
            escaped << L'%' << uppercase << setw(2)
                << int(static_cast<unsigned char>(c))
                << nouppercase;
        }
    }

    return escaped.str();
}

wstring GetAppVersion()
{
    wchar_t filename[MAX_PATH];
    GetModuleFileNameW(NULL, filename, MAX_PATH);

    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(filename, &handle);
    if (size == 0)
        return L"";

    wstring version;
    vector<BYTE> data(size);

    if (GetFileVersionInfoW(filename, handle, size, data.data()))
    {
        VS_FIXEDFILEINFO* fileInfo = nullptr;
        UINT len = 0;
        if (VerQueryValueW(data.data(), L"\\", reinterpret_cast<LPVOID*>(&fileInfo), &len))
        {
            if (fileInfo)
            {
                int major = HIWORD(fileInfo->dwFileVersionMS);
                int minor = LOWORD(fileInfo->dwFileVersionMS);
                int patch = HIWORD(fileInfo->dwFileVersionLS);
                int build = LOWORD(fileInfo->dwFileVersionLS);

                wchar_t buf[50];
                swprintf_s(buf, L"%d.%d.%d.%d", major, minor, patch, build);
                version = buf;
            }
        }
    }

    return version;
}

wstring GetPluginVersion(HMODULE hModule)
{
    wchar_t filename[MAX_PATH];
    if (GetModuleFileNameW(hModule, filename, MAX_PATH) == 0)
        return L"";

    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(filename, &handle);
    if (size == 0)
        return L"";

    vector<BYTE> data(size);
    if (!GetFileVersionInfoW(filename, handle, size, data.data()))
        return L"";

    VS_FIXEDFILEINFO* fileInfo = nullptr;
    UINT len = 0;
    if (VerQueryValueW(data.data(), L"\\", reinterpret_cast<LPVOID*>(&fileInfo), &len) && fileInfo)
    {
        int major = HIWORD(fileInfo->dwFileVersionMS);
        int minor = LOWORD(fileInfo->dwFileVersionMS);
        int patch = HIWORD(fileInfo->dwFileVersionLS);
        int build = LOWORD(fileInfo->dwFileVersionLS);

        wchar_t buf[50];
        swprintf_s(buf, L"%d.%d.%d.%d", major, minor, patch, build);
        return buf;
    }

    return L"";
}