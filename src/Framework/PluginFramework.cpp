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

#include "PluginFramework.h"

PluginData plugin;

// DLL standard interface and some Notepad++ plugin housekeeping that never changes

BOOL APIENTRY DllMain(HINSTANCE instance, DWORD reasonForCall, LPVOID) {
    try {
        switch (reasonForCall) {
            case DLL_PROCESS_ATTACH:
                plugin.dllInstance         = instance;
                plugin.bypassNotifications = false;
                plugin.fileIsOpening       = false;
                plugin.startupOrShutdown   = true;
                break;

            case DLL_PROCESS_DETACH:
                break;

            case DLL_THREAD_ATTACH:
                break;

            case DLL_THREAD_DETACH:
                break;
        }
    }
    catch (...) { return FALSE; }
    return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NPP::NppData nppData) {
    plugin.nppData = nppData;
    plugin.directStatusScintilla = reinterpret_cast<Scintilla::FunctionDirect>
        (SendMessage(plugin.nppData._scintillaMainHandle, static_cast<UINT>(Scintilla::Message::GetDirectStatusFunction), 0, 0));
}

extern "C" __declspec(dllexport) BOOL isUnicode() {return TRUE;}
