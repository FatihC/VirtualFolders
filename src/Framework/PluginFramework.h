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

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

#include "ScintillaCallEx.h"

namespace NPP {
    #include "../Host/PluginInterface.h"
    #include "../Host/menuCmdID.h"
    #include "../Host/Docking.h"
}


extern struct PluginData plugin;  // Instantiated in PluginFramework.cpp

struct PluginData {

    NPP::NppData              nppData;                     // handles exposed by Notepad++
    HINSTANCE                 dllInstance;                 // Instance handle of this DLL
    Scintilla::FunctionDirect directStatusScintilla;       // Scintilla direct function address for ScintillaCall interface
    intptr_t                  pointerScintilla;            // Scintilla direct function pointer
    Scintilla::ScintillaCall  sci;                         // Scintilla C++ interface
    bool                      bypassNotifications = false; // Avoid processing notifications, because we're causing them
    bool                      fileIsOpening       = false; // A new file is opening
    bool                      startupOrShutdown   = true;  // Notepad++ is starting up or shutting down

    HWND currentScintilla() const {
        int currentEdit = 0;
        SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&currentEdit));
        return currentEdit ? nppData._scintillaSecondHandle : nppData._scintillaMainHandle;
    }

    void getScintillaPointers() {
        getScintillaPointers(currentScintilla());
    }

    void getScintillaPointers(const Scintilla::NotificationData* scnp) {
        getScintillaPointers(reinterpret_cast<HWND>(scnp->nmhdr.hwndFrom));
    }

    void getScintillaPointers(HWND scintillaHandle) {
        pointerScintilla = SendMessage(scintillaHandle, static_cast<UINT>(Scintilla::Message::GetDirectPointer), 0, 0);
        sci.SetFnPtr(directStatusScintilla, pointerScintilla);
        sci.SetStatus(Scintilla::Status::Ok);  // C-interface code can ignore an error status, causing exception in C++ interface
    }

    // cmd calls menu commands with notifications bypassed and Scintilla pointers established

    void cmd(void (cmdFunction)()) { 
        bypassNotifications = true;
        getScintillaPointers();
        (cmdFunction)();
        bypassNotifications = false;
    }

};
