// This file is part of "NppCppMSVS: Visual Studio Project Template for a Notepad++ C++ Plugin"
// Copyright 2025 by Randall Joseph Fellmy <software@coises.com>, <http://www.coises.com/software/>

// The source code contained in this file is independent of Notepad++ code.
// It is released under the MIT (Expat) license:
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and 
// associated documentation files (the "Software"), to deal in the Software without restriction, 
// including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, 
// subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial 
// portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT 
// LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


// Template "config" and structs "config_history" and "config_rect" for managing configuration settings

#pragma once

#include <string>
#include <vector>
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include "../nlohmann/json.hpp"


// Compare strings for equality excluding trailing blanks

template<typename T>bool equalExceptTrailing(T s1, T s2) {
    size_t n = s1.find_last_not_of(' ');
    if (n != s2.find_last_not_of(' ')) return false;
    if (n == std::wstring::npos) return true;
    return s1.substr(0, n) == s2.substr(0, n);
}


extern nlohmann::json configuration;  // persistent data (in sample project, instantiated and read/written in Configuration.cpp)

// Specialization of nlohmann::adl_serializer for std::wstring

namespace nlohmann {
    template <>
    struct adl_serializer<std::wstring> {
        static void to_json(json& j, const std::wstring& w) {
            if (w.empty()) j = "";
            else {
                std::string s(WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.length()), 0, 0, 0, 0), 0);
                WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.length()), s.data(), static_cast<int>(s.length()), 0, 0);
                j = s;
            }
        }
        static void from_json(const json& j, std::wstring& w) {
            std::string s = j.get<std::string>();
            if (s.empty()) w = L"";
            else {
                w.resize(MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.length()), 0, 0));
                MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.length()), w.data(), static_cast<int>(w.length()));
            }
        }
    };
}


// Definition of template "config"

template<typename T> struct config {

    std::string name;
    nlohmann::json* store;
    T value;
    bool loaded = false;

    static bool peek(T& v, const nlohmann::json& j, std::string_view n);
    static bool peek(T& v, HWND w)                                     ;
    static bool peek(T& v, HWND w, int id) { return peek(v, GetDlgItem(w, id)); }

    T peek(const nlohmann::json& j, std::string_view n) const { T v; return peek(v, j, n) ? v : value; }
    T peek(HWND w)                                      const { T v; return peek(v, w) ? v : value; }
    T peek(HWND w, int id)                              const { return peek(GetDlgItem(w, id)); }

    static void show(HWND w, const T& v);
    static void show(HWND w, int id, const T& v) { return show(GetDlgItem(w, id), v); }

    T& get(const nlohmann::json& j, std::string_view n) { loaded |= peek(value, j, n); return value; }
    T& get(HWND w)         { if (peek(value, w)) { loaded = true; if (store && !name.empty()) put(*store, name); } return value; }
    T& get(HWND w, int id) { return get(GetDlgItem(w, id)); }
    T& get()               { if (!loaded && store && !name.empty()) { get(*store, name); loaded = true; } return value; }

    const T& put(nlohmann::json& j, std::string_view n) const { j[n] = value; return value; }
    const T& put(HWND w)                                      { show(w, get()); return value; }
    const T& put(HWND w, int id)                              { return put(GetDlgItem(w, id)); }

    operator T&()                    { return get(); }
    config<T>& operator=(const T& v) { value = v; loaded = true; if (store && !name.empty()) put(*store, name); return *this; }

    config(const T& initial) : name(""), store(0), value(initial) {}

    config(const std::string& name, const T& initial, nlohmann::json& store = configuration)
        : name(name), store(&store), value(initial) {}

};


template<typename T> bool config<T>::peek(T& v, const nlohmann::json& j, std::string_view n) {
    if (!j.contains(n)) return false;
    auto& e = j[n];
    try {
        v = e.get<T>();
    }
    catch (...) {
        return false;
    }
    return true;
}

template<typename T>  bool config<T>::peek(T& v, HWND w) {
    if constexpr (std::is_same_v<T, bool>) {
        auto state = SendMessage(w, BM_GETCHECK, 0, 0);
        if (state == BST_INDETERMINATE) return false;
        v = state == BST_CHECKED;
        return true;
    }
    else if constexpr (std::is_same_v<T, std::wstring>) {
        v.resize(GetWindowTextLength(w), 0);
        if (!v.empty()) v.resize(GetWindowText(w, v.data(), static_cast<int>(v.length() + 1)));
        return true;
    }
    else if constexpr (std::is_arithmetic_v<T>) {
        if constexpr (std::is_integral_v<T>) {
            std::wstring classname(127, 0);
            GetClassName(w, classname.data(), static_cast<int>(classname.length() + 1));
            classname.resize(std::min(classname.find(L'\0'), classname.length()));
            if (classname == UPDOWN_CLASS) {
                BOOL error = 0;
                T n = static_cast<T>(SendMessage(w, UDM_GETPOS32, 0, reinterpret_cast<LPARAM>(&error)));
                if (error) return false;
                v = n;
                return true;
            }
        }
        std::wstring text(GetWindowTextLength(w), 0);
        if (text.empty()) return false;
        text.resize(GetWindowText(w, text.data(), static_cast<int>(text.length() + 1)));
        errno = 0;
        if constexpr (std::is_floating_point_v<T>) {
            long double result = wcstold(text.data(), 0);
            if (errno) {
                errno = 0;
                return false;
            }
            if (result < std::numeric_limits<T>::min() || result > std::numeric_limits<T>::max()) return false;
            v = static_cast<T>(result);
            return true;
        }
        else if constexpr (std::is_signed_v<T>) {
            long long int result = wcstoll(text.data(), 0, 10);
            if (errno) {
                errno = 0;
                return false;
            }
            if (result < std::numeric_limits<T>::min() || result > std::numeric_limits<T>::max()) return false;
            v = static_cast<T>(result);
            return true;
        }
        else {
            long long unsigned int result = wcstoull(text.data(), 0, 10);
            if (errno) {
                errno = 0;
                return false;
            }
            if (result > std::numeric_limits<T>::max()) return false;
            v = static_cast<T>(result);
            return true;
        }
    }
    else static_assert(false, "config template type not supported for this operation");
}

template<typename T> void config<T>::show(HWND w, const T& v) {
    if constexpr (std::is_same_v<T, bool>) {
        SendMessage(w, BM_SETCHECK, v ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    else if constexpr (std::is_same_v<T, std::wstring>) {
        SetWindowText(w, v.data());
    }
    else if constexpr (std::is_arithmetic_v<T>) {
        if constexpr (std::is_integral_v<T>) {
            std::wstring classname(127, 0);
            GetClassName(w, classname.data(), static_cast<int>(classname.length() + 1));
            classname.resize(std::min(classname.find(L'\0'), classname.length()));
            if (classname == UPDOWN_CLASS) {
                SendMessage(w, UDM_SETPOS32, 0, v);
                return;
            }
        }
        SetWindowText(w, std::to_wstring(v).data());
    }
    else static_assert(false, "config template type not supported for this operation");
}


// Definition of config_history, std::wstring with history / combobox

struct config_history {

    std::string name;
    nlohmann::json* store;
    std::vector<std::wstring> history;
    int depth;
    bool retainBlank;
    bool retainDuplicate;
    bool retainEmpty;
    bool loaded = false;

          std::wstring& value()       { if (history.empty()) history = { L"" };  return history[0]; }
    const std::wstring& value() const { if (history.empty()) return L"";  return history[0]; }

    static bool peek(std::vector<std::wstring>& v, const nlohmann::json& j, std::string_view n);

    static std::wstring peek(HWND w);
    static std::wstring peek(HWND w, int id) { return peek(GetDlgItem(w, id)); }

    static void show(HWND w, const std::wstring& s)         { SetWindowText(w, s.data()); }
    static void show(HWND w, int id, const std::wstring& s) { return show(GetDlgItem(w, id), s); }

    std::wstring& get(const nlohmann::json& j, std::string_view n) { loaded |= peek(history, j, n); return value(); }
    std::wstring& get(HWND w);
    std::wstring& get(HWND w, int id) { return get(GetDlgItem(w, id)); }
    std::wstring& get()               { if (!loaded && store && !name.empty()) { get(*store, name); loaded = true; } return value(); }

    const std::wstring& put(nlohmann::json& j, std::string_view n) const;
    const std::wstring& put(HWND w);
    const std::wstring& put(HWND w, int id)    { return put(GetDlgItem(w, id)); }

    operator std::wstring&() { return get(); }
    config_history& operator=(const std::vector<std::wstring>& v);
    config_history& operator=(const std::wstring& v);
    config_history& operator+=(const std::wstring& v);

    constexpr static int Blank     = 1;
    constexpr static int Duplicate = 2;
    constexpr static int Empty     = 4;

    config_history(const std::vector<std::wstring>& initial, int depth = 10, int retain = 0)
        : name(""), store(0), history(initial), depth(std::max(0, depth)),
          retainBlank(retain & Blank), retainDuplicate(retain & Duplicate), retainEmpty(retain & Empty) {}

    config_history(const std::string& name, const std::vector<std::wstring>& initial = {},
                   int depth = 10, int retain = 0, nlohmann::json& store = configuration)
        : name(name), store(&store), history(initial), depth(std::max(0, depth)),
          retainBlank(retain & Blank), retainDuplicate(retain & Duplicate), retainEmpty(retain & Empty) {}

};


inline bool config_history::peek(std::vector<std::wstring>& v, const nlohmann::json& j, std::string_view n) {
    if (!j.contains(n)) return false;
    auto& a = j[n];
    if (!a.is_array()) return false;
    try {
        v = a.get<std::vector<std::wstring>>();
    }
    catch (...) {
        return false;
    }
    return true;
}

inline std::wstring config_history::peek(HWND w) {
    std::wstring editText(GetWindowTextLength(w), 0);
    if (!editText.empty()) editText.resize(GetWindowText(w, editText.data(), static_cast<int>(editText.length() + 1)));
    return editText;
}

inline std::wstring& config_history::get(HWND w) {
    auto listCount = SendMessage(w, CB_GETCOUNT, 0, 0);
    if (listCount < 0) {
        if (history.empty()) history.push_back(L"");
        return history[0];
    }
    history.clear();
    std::wstring editText(GetWindowTextLength(w), 0);
    if (!editText.empty()) editText.resize(GetWindowText(w, editText.data(), static_cast<int>(editText.length() + 1)));
    history.push_back(editText);
    int item = 0;
    bool addText = retainEmpty || (retainBlank ? !editText.empty() : editText.find_first_not_of(L' ') != std::wstring::npos);
    if (addText && listCount && !retainDuplicate) {
        std::wstring itemText(SendMessage(w, CB_GETLBTEXTLEN, 0, 0), 0);
        if (!itemText.empty()) itemText.resize(SendMessage(w, CB_GETLBTEXT, 0, reinterpret_cast<LPARAM>(itemText.data())));
        if (itemText == editText) {
            addText = false;
            item = 1;
        }
    }
    if (addText) {
        SendMessage(w, CB_INSERTSTRING, 0, reinterpret_cast<LPARAM>(editText.data()));
        ++listCount;
        item = 1;
    }
    for (; item < listCount; ++item) {
        if (depth && item >= depth) {
            while (listCount > depth) SendMessage(w, CB_DELETESTRING, --listCount, 0);
            break;
        }
        std::wstring itemText(SendMessage(w, CB_GETLBTEXTLEN, item, 0), 0);
        if (!itemText.empty()) itemText.resize(SendMessage(w, CB_GETLBTEXT, item, reinterpret_cast<LPARAM>(itemText.data())));
        if ( (!retainEmpty && (retainBlank ? itemText.empty() : itemText.find_first_not_of(L' ') == std::wstring::npos))
          || (!retainDuplicate && (retainBlank ? itemText == editText : equalExceptTrailing(itemText, editText))) ) {
            SendMessage(w, CB_DELETESTRING, item, 0);
            --item;
            --listCount;
        }
        else history.push_back(itemText);
    }
    loaded = true;
    if (store && !name.empty()) put(*store, name);
    return history[0];
}

inline const std::wstring& config_history::put(nlohmann::json& j, std::string_view n) const {
    static const std::wstring empty;
    if (history.empty()) {
        j[n] = { "" };
        return empty;
    }
    j[n] = history;
    return history[0];
}

inline const std::wstring& config_history::put(HWND w) {
    get();
    SendMessage(w, CB_RESETCONTENT, 0, 0);
    for (const auto& s : history)
        if (s.empty() ? retainEmpty : retainBlank || s.find_first_not_of(L' ') != std::wstring::npos) 
            SendMessage(w, CB_INSERTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(s.data()));
    if (!history.empty() && !history[0].empty()) {
        if (!retainBlank && history[0].find_first_not_of(L' ') == std::wstring::npos) {
            COMBOBOXINFO cbi;
            cbi.cbSize = sizeof(COMBOBOXINFO);
            if (GetComboBoxInfo(w, &cbi)) SetWindowText(cbi.hwndItem, history[0].data());
        }
        else SendMessage(w, CB_SETCURSEL, 0, 0);
    }
    return value();
}

inline config_history& config_history::operator=(const std::vector<std::wstring>& v) {
    history = v;
    loaded = true;
    if (store && !name.empty()) put(*store, name);
    return *this;
}

inline config_history& config_history::operator=(const std::wstring& v) {
    if (history.empty()) history.push_back(v);
    else                 history[0] = v;
    loaded = true;
    if (store && !name.empty()) put(*store, name);
    return *this;
}

inline config_history& config_history::operator+=(const std::wstring& v) {
    if (history.empty()) history.push_back(v);
    else if (retainDuplicate) {
        if (!retainEmpty && (retainBlank ? history[0].empty() : history[0].find_first_not_of(L' ') != std::wstring::npos))
             history[0] = v;
        else history.insert(history.begin(), v);
    }
    else if (v != history[0]) {
        if ( (!retainEmpty && (retainBlank ? history[0].empty() : history[0].find_first_not_of(L' ') != std::wstring::npos))
          || (!retainBlank && equalExceptTrailing(v, history[0])) )
             history[0] = v;
        else history.insert(history.begin(), v);
        if (history.size() > 1) {
            for (auto it = history.begin() + 1; it != history.end();) {
                if (retainBlank ? v == *it : equalExceptTrailing(v, *it)) it = history.erase(it);
                else ++it;
            }
        }
    }
    if (depth && static_cast<int>(history.size()) > depth) history.resize(depth);
    loaded = true;
    if (store && !name.empty()) put(*store, name);
    return *this;
}


// Definition of config_rect, for saving and restoring dialog or other top-level window positions

struct config_rect {

    std::string name;
    nlohmann::json* store;
    RECT value = { 0, 0, 0, 0 };
    bool loaded = false;

    static bool peek(RECT& v, const nlohmann::json& j, std::string_view n);
    static bool peek(RECT& v, HWND w);

    RECT peek(const nlohmann::json& j, std::string_view n) const { RECT v; return peek(v, j, n) ? v : value; }
    RECT peek(HWND w)                                      const { RECT v; return peek(v, w) ? v : value; }

    static void show(HWND w, const RECT& v = RECT());

    RECT& get(const nlohmann::json& j, std::string_view n) { loaded |= peek(value, j, n); return value; }
    RECT& get(HWND w) { if (peek(value, w)) { loaded = true; if (store && !name.empty()) put(*store, name); } return value; }
    RECT& get()       { if (!loaded && store && !name.empty()) { get(*store, name); loaded = true; } return value; }

    const RECT& put(nlohmann::json& j, std::string_view n) const;
    const RECT& put(HWND w) { show(w, get()); return value; }

    operator RECT& () { return get(); }
    config_rect& operator=(const RECT& v) { value = v; loaded = true; if (store && !name.empty()) put(*store, name); return *this; }

    config_rect() : name(""), store(0) {}
    config_rect(const std::string& name, nlohmann::json& store = configuration) : name(name), store(&store) {}

};

inline bool config_rect::peek(RECT& v, const nlohmann::json& j, std::string_view n) {
    if (!j.contains(n)) return false;
    auto& e = j[n];
    if (!e.is_array() || e.size() != 4) return false;
    try {
        LONG v0 = e[0].get<LONG>();
        LONG v1 = e[1].get<LONG>();
        LONG v2 = e[2].get<LONG>();
        LONG v3 = e[3].get<LONG>();
        v = { v0, v1, v2, v3 };
    }
    catch (...) {
        return false;
    }
    return true;
}

inline bool config_rect::peek(RECT& v, HWND w) {
    RECT r;
    if (!GetWindowRect(w, &r)) return false;
    HWND owner = GetParent(w);
    if (owner) MapWindowPoints(0, owner, reinterpret_cast<LPPOINT>(&r), 2);
    v = r;
    return true;
}

inline void config_rect::show(HWND w, const RECT& v) {
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    HWND owner = GetParent(w);
    RECT r;
    if (v.top >= v.bottom || v.left >= v.right) /* Center on owner client area, or workarea if not owned */ {
        RECT c;
        if (owner) {
            GetClientRect(owner, &c);
            MapWindowPoints(owner, 0, reinterpret_cast<LPPOINT>(&c), 2);
            GetMonitorInfo(MonitorFromRect(&c, MONITOR_DEFAULTTOPRIMARY), &mi);
        }
        else {
            GetMonitorInfo(MonitorFromWindow(GetActiveWindow(), MONITOR_DEFAULTTOPRIMARY), &mi);
            c = mi.rcWork;
        }
        GetWindowRect(w, &r);
        int dx = (c.left + c.right - r.left - r.right) / 2;
        int dy = (c.top + c.bottom - r.top - r.bottom) / 2;
        r.left   += dx;
        r.right  += dx;
        r.top    += dy;
        r.bottom += dy;
    }
    else /* restore last position and size */ {
        r = v;
        if (owner) MapWindowPoints(owner, 0, reinterpret_cast<LPPOINT>(&r), 2);
        GetMonitorInfo(MonitorFromRect(&r, MONITOR_DEFAULTTOPRIMARY), &mi);
    }
    int width  = std::min(r.right - r.left, mi.rcWork.right - mi.rcWork.left);
    int height = std::min(r.bottom - r.top, mi.rcWork.bottom - mi.rcWork.top);
    int left   = std::max(mi.rcWork.left, std::min(r.left, mi.rcWork.right - width));
    int top    = std::max(mi.rcWork.top, std::min(r.top, mi.rcWork.bottom - height));
    SetWindowPos(w, 0, left, top, width, height, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
}

inline const RECT& config_rect::put(nlohmann::json& j, std::string_view n) const {
    j[n] = nlohmann::json::array({ value.left, value.top, value.right, value.bottom });
    return value;
}
