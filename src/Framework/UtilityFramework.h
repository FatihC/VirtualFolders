// This file is part of "NppCppMSVS: Visual Studio Project Template for a Notepad++ C++ Plugin"
// Copyright 2025 by Randall Joseph Fellmy <software@coises.com>, <http://www.coises.com/software/>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.


// Inline utility / helper routines -- for convenience; none are required by Notepad++ or Scintilla


#pragma once
#include "PluginFramework.h"
#include "UtilityFrameworkMIT.h"


// Sending messages to Notepad++ and to Scintilla

template<typename W, typename L> LRESULT npp(UINT Msg, W wParam, L lParam) {
    if constexpr (std::is_integral_v<W> || std::is_enum_v<W>)
        if constexpr (std::is_integral_v<L> || std::is_enum_v<L>)
             return SendMessage(plugin.nppData._nppHandle, Msg, static_cast<WPARAM>(wParam), static_cast<LPARAM>(lParam));
        else return SendMessage(plugin.nppData._nppHandle, Msg, static_cast<WPARAM>(wParam), reinterpret_cast<LPARAM>(lParam));
    else if constexpr (std::is_integral_v<L> || std::is_enum_v<L>)
         return SendMessage(plugin.nppData._nppHandle, Msg, reinterpret_cast<WPARAM>(wParam), static_cast<LPARAM>(lParam));
    else return SendMessage(plugin.nppData._nppHandle, Msg, reinterpret_cast<WPARAM>(wParam), reinterpret_cast<LPARAM>(lParam));
}

inline Scintilla::ScintillaCall& sci = plugin.sci;


// Convert between Windows UTF-16 strings and Scintilla's ANSI or UTF-8 strings, even if they are very long.

inline std::string  fromWide(std::wstring_view s) { return fromWide(s, plugin.sci.CodePage()); }
inline std::wstring toWide  (std::string_view  s) { return   toWide(s, plugin.sci.CodePage()); }


// Get the full path of a file open in Notepad++ as a std::wstring.
// If an argument is supplied, it is the Notepad++ buffer id to be examined.
// If no argument is given, the current buffer is examined; this only works in commands and NPPN_BUFFERACTIVATED notifications,
// since in Scintilla notifications and most Notepad++ notifications, the "current buffer" is not necessarily meaningful.

inline std::wstring getFilePath(UINT_PTR buffer) {
    auto n = SendMessage(plugin.nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, buffer, 0);
    if (n < 1) return L"";
    std::wstring s(n, 0);
    SendMessage(plugin.nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, buffer, (LPARAM)s.data());
    return s;
}
inline std::wstring getFilePath() { return getFilePath(SendMessage(plugin.nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0)); }


// Get the lower case file extension from a path, a buffer id or the current buffer.
// Returns L"." for a file with no extension, L"" for a non-path (i.e., without a backslash, e.g., "new 1").
// If no argument is given, the current buffer is examined; this only works in commands and NPPN_BUFFERACTIVATED notifications,
// since in Scintilla notifications and most Notepad++ notifications, the "current buffer" is not necessarily meaningful.

inline std::wstring getFileExtension(const std::wstring& filepath) {
    size_t lastBackslash = filepath.find_last_of(L'\\');
    if (lastBackslash == std::wstring::npos) return L"";
    size_t lastPeriod = filepath.find_last_of(L'.');
    if (lastPeriod == std::wstring::npos || lastPeriod < lastBackslash || lastPeriod == filepath.length() - 1) return L".";
    std::wstring ext = filepath.substr(lastPeriod + 1);
    wcslwr(ext.data());
    return ext;
}
inline std::wstring getFileExtension(UINT_PTR buffer) { return getFileExtension(getFilePath(buffer)); }
inline std::wstring getFileExtension() { return getFileExtension(SendMessage(plugin.nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0)); }
