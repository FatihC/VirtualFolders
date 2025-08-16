// This file is part of VFolders.
// Copyright 2025 by FatihCoskun.

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

#include "Framework/PluginFramework.h"
#include <iostream>
#include "CommonData.h"

using namespace NPP;


// Routines that load and save the configuration file

void loadConfiguration();
void saveConfiguration();

// Routines that process Scintilla notifications

void scnModified(const Scintilla::NotificationData*);
void scnUpdateUI(const Scintilla::NotificationData*);
void scnZoom(const Scintilla::NotificationData*);

// Routines that process Notepad++ notifications

void bufferActivated();
void beforeFileClose(const NMHDR*);
void fileClosed(const NMHDR*);
void fileOpened(const NMHDR*);
void modifyAll(const NMHDR*);
void nppReady();
void nppShutdown();

// Routines that process menu commands

void printListOpenFiles();
std::vector<std::string> listOpenFiles();
void showAboutDialog();
void showSettingsDialog();
void toggleStatusDialog();
void toggleWatcherPanel();
void toggleWatcherPanelWithList();
void resizeWatcherPanel();


// External variables
extern HWND watcherPanel;

// External functions
extern void updateTreeColorsExternal(HWND hTree);

// Name and define any shortcut keys to be assigned as menu item defaults: Ctrl, Alt, Shift and the virtual key code
//
// Define menu commands:
//     text to appear on menu (ignored for a menu separator line)
//     address of a void, zero-argument function that processes the command (0 for a menu separator line)
//         recommended: use plugin.cmd, per examples, to wrap the function, setting Scintilla pointers and bypassing notifications
//     ignored on call; on return, Notepad++ will fill this in with the menu command ID it assigns
//     whether to show a checkmark beside this item on initial display of the menu
//     0 or default shortcut key (menu accelerator) specified as the address of an NPP::ShortcutKey structure
//
// If you will reference any menu items elsewhere, define mnemonic references for them following the menu definition,
// where it's easy to see and update them if you change the menu; then you can use:
//    extern NPP::FuncItem menuDefinition[];
//    extern int mnemonic;
// in other source files; use the mnemonic to get the ordinal position in the original menu, and:
//    menuDefinition[mnemonic]._cmdID
// to get the menu item identifier assigned by Notepad++.

static ShortcutKey SKToggleStatus { true, true, true, VK_HOME };

FuncItem menuDefinition[] = {
    { L"Insert List of Open Files", []() {plugin.cmd(printListOpenFiles);}, 0, false, 0               },
    { L"Show Status"              , []() {plugin.cmd(toggleStatusDialog);}, 0, false, &SKToggleStatus },
    { L"Show Watcher Panel"       , []() {plugin.cmd(toggleWatcherPanelWithList);}, 0, false, 0               },
    { 0                           , 0                                     , 0, false, 0               },
    { L"Settings..."              , []() {plugin.cmd(showSettingsDialog);}, 0, false, 0               },
    { L"Help/About..."            , []() {plugin.cmd(showAboutDialog   );}, 0, false, 0               },
};

int menuItem_ToggleStatus  = 1;
int menuItem_ToggleWatcher = 2;


// Tell Notepad++ the plugin name

extern "C" __declspec(dllexport) const wchar_t* getName() {
    return L"VFolders";
}


// Tell Notepad++ about the plugin menu

extern "C" __declspec(dllexport) FuncItem * getFuncsArray(int *n) {
    loadConfiguration();
    *n = sizeof(menuDefinition) / sizeof(FuncItem);
    return reinterpret_cast<FuncItem*>(&menuDefinition);
}


// Notification processing: each notification desired must be sent to a function that will handle it.

extern "C" __declspec(dllexport) void beNotified(SCNotification *np) {

    if (plugin.bypassNotifications) return;
    plugin.bypassNotifications = true;
    auto*& nmhdr = reinterpret_cast<NMHDR*&>(np);

    // Example Notepad++ notifications; you can add others as needed.
    // Note that most of the notifications listed below have some connection to plugin framework code;
    // it's best to leave those and just remove any function calls you don't use.

    if (nmhdr->hwndFrom == plugin.nppData._nppHandle) {
      
        switch (nmhdr->code) {

        case NPPN_BEFORESHUTDOWN:
            plugin.startupOrShutdown = true;
            break;

        case NPPN_BUFFERACTIVATED:
            if (!plugin.startupOrShutdown && !plugin.fileIsOpening) {
                plugin.getScintillaPointers();
                bufferActivated();
            }
            break;

        case NPPN_CANCELSHUTDOWN:
            plugin.startupOrShutdown = false;
            break;

        case NPPN_FILEBEFOREOPEN:
            plugin.fileIsOpening = true;
            break;

        case NPPN_FILEBEFORECLOSE:
            beforeFileClose(nmhdr);
            break;

        case NPPN_FILECLOSED:
            fileClosed(nmhdr);
            break;
        
        case NPPN_FILEOPENED:
            plugin.fileIsOpening = false;
            fileOpened(nmhdr);
            break;

        case NPPN_GLOBALMODIFIED:
            modifyAll(nmhdr);
            break;

        case NPPN_DARKMODECHANGED:
            // Handle dark mode change - update any open dialogs
            if (watcherPanel && IsWindow(watcherPanel)) {
                npp(NPPM_DARKMODESUBCLASSANDTHEME, NPP::NppDarkMode::dmfHandleChange, watcherPanel);
                SetWindowPos(watcherPanel, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
                
                // Update tree colors
                HWND hTree = GetDlgItem(watcherPanel, 1023);
                if (hTree) {
                    updateTreeColorsExternal(hTree);
                }
                
                // Resize the panel to match the new theme
                resizeWatcherPanel();
            }
            break;

        case NPPN_READY:
            // If you use Scintilla::Notification::Modified, send the following message to tell Notepad++
            // which events you need; https://www.scintilla.org/ScintillaDoc.html#SCN_MODIFIED lists them.
            // Note that this does not mean you will not get messages for other events; it only specifies that
            // you do want at least the ones you list.  You still must test and return quickly from messages
            // you don't need.  If you don't use Scintilla::Notification::Modified, remove the next line.
            SendMessage(plugin.nppData._nppHandle, NPPM_ADDSCNMODIFIEDFLAGS, 0, SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT);                  
            plugin.startupOrShutdown = false;
            plugin.getScintillaPointers();
            bufferActivated();
            nppReady(); // Sync vData when Notepad++ is ready
            break;

        case NPPN_SHUTDOWN:
            nppShutdown();
            saveConfiguration();
            break;

        }

    }

    // Example Scintilla notifications; use only the notifications you need.

    else if (nmhdr->hwndFrom == plugin.nppData._scintillaMainHandle || nmhdr->hwndFrom == plugin.nppData._scintillaSecondHandle) {

        auto*& scnp = reinterpret_cast<Scintilla::NotificationData*&>(np);
        switch (scnp->nmhdr.code) {

        case Scintilla::Notification::Modified:
            plugin.getScintillaPointers(scnp);
            scnModified(scnp);
            break;

        case Scintilla::Notification::UpdateUI:
            plugin.getScintillaPointers(scnp);
            scnUpdateUI(scnp);
            break;

        case Scintilla::Notification::Zoom:
            plugin.getScintillaPointers(scnp);
            scnZoom(scnp);
            break;

        default:;
        }

    }

    plugin.bypassNotifications = false;

}


// This is rarely used, but a few Notepad++ commands call this routine as part of their processing

extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) {return TRUE;}
