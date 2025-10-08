// This file is part of VirtualFolders.
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
void loadLocalization();
void loadShortcuts(string langFilePath);


// Routines that process Scintilla notifications

void scnModified(const Scintilla::NotificationData*);
void scnUpdateUI(const Scintilla::NotificationData*);
void scnZoom(const Scintilla::NotificationData*);
void scnSavePointEvent(UINT_PTR bufferID, bool isSavePoint);

// Routines that process Notepad++ notifications


void bufferActivated(const NMHDR* nmhdr);
void beforeFileClose(const NMHDR*);
void fileClosed(const NMHDR*);
void fileOpened(const NMHDR*);
void fileRenamed(const NMHDR*);
void fileSaved(const NMHDR*);
void readOnlyChanged(const NMHDR*);
void modifyAll(const NMHDR*);
void nppReady();
void nppBeforeShutdown();
void nppShutdown();

// Routines that process menu commands


std::vector<std::string> listOpenFiles();
void showAboutDialog();
void showSettingsDialog();
void toggleVirtualPanelWithList();
void toggleShortcutOverride();
void resizeVirtualPanel();
void increaseFontSize();
void decreaseFontSize();
void setFontSize();




// External variables
extern HWND virtualPanelWnd;

// External functions
extern void updateTreeColorsExternal(HWND hTree);
extern void loadMenus();



NPP::ShortcutKey SKNextTab;


int menuItem_ToggleVirtualPanel = 0;
int menuItem_Settings = 1;
int menuItem_ShortcutOverrider = 2;
int menuItem_IncreaseFont = 3;
int menuItem_DecreaseFont = 4;
int menuItem_About = 5;



FuncItem menuDefinition[] = {
    { L"Show Virtual Folder Panel", []() {plugin.cmd(toggleVirtualPanelWithList);},    menuItem_ToggleVirtualPanel,       false,      0},
    { L"Settings..."              , []() {plugin.cmd(showSettingsDialog);},         menuItem_Settings,              false,      0},
    { L"Override ShortCuts"       , []() {plugin.cmd(toggleShortcutOverride);},     menuItem_ShortcutOverrider,     plugin.isShortcutOverridden,    0},
    { L"Increase Plugin Font Size: 9px", []() {plugin.cmd(increaseFontSize); },     menuItem_IncreaseFont,          false,      0},
    { L"Decrease Plugin Font Size: 9px", []() {plugin.cmd(decreaseFontSize); },     menuItem_DecreaseFont,          false,      0},
    { L"Help/About..."            , []() {plugin.cmd(showAboutDialog   );},         menuItem_About,                 false,      0},
};





// Tell Notepad++ the plugin name

extern "C" __declspec(dllexport) const wchar_t* getName() {
    return L"VirtualFolders";
}


// Tell Notepad++ about the plugin menu

extern "C" __declspec(dllexport) FuncItem * getFuncsArray(int *n) {
    loadConfiguration();
	loadLocalization();

    if (virtualPanelWnd) {
		wstring title = commonData.translator->getTextW("IDM_PLUGIN_TITLE");
        ::SetWindowTextW(virtualPanelWnd, title.c_str());
    }

    std::wstring showVirtualPanelLabel = commonData.translator->getTextW("IDM_SHOW_VIRTUAL_PANEL");
    wcsncpy_s(menuDefinition[menuItem_ToggleVirtualPanel]._itemName, menuItemSize, showVirtualPanelLabel.c_str(), _TRUNCATE);

    std::wstring settingsLabel = commonData.translator->getTextW("IDM_SETTINGS");
    wcsncpy_s(menuDefinition[menuItem_Settings]._itemName, menuItemSize, settingsLabel.c_str(), _TRUNCATE);

    std::wstring shortcutsLabel = commonData.translator->getTextW("IDM_SHORTCUT_OVERRIDE");
    wcsncpy_s(menuDefinition[menuItem_ShortcutOverrider]._itemName, menuItemSize, shortcutsLabel.c_str(), _TRUNCATE);

    std::wstring aboutLabel = commonData.translator->getTextW("IDM_ABOUT");
    wcsncpy_s(menuDefinition[menuItem_About]._itemName, menuItemSize, aboutLabel.c_str(), _TRUNCATE);



       
    if (plugin.isShortcutOverridden) {
		plugin.isShortcutOverridden = false; // temporary
        toggleShortcutOverride(); // PluginFrameWork.toggleShortcutOverride
    }
    std::wstring fontIncreaseLabel = commonData.translator->getTextW("IDM_FONT_INCREASE") + std::to_wstring(commonData.fontSize) + L" px";
    wcsncpy_s(menuDefinition[menuItem_IncreaseFont]._itemName, menuItemSize, fontIncreaseLabel.c_str(),_TRUNCATE);

    std::wstring fontDecreaseLabel = commonData.translator->getTextW("IDM_FONT_DECREASE") + std::to_wstring(commonData.fontSize) + L" px";
    wcsncpy_s(menuDefinition[menuItem_DecreaseFont]._itemName, menuItemSize, fontDecreaseLabel.c_str(), _TRUNCATE);



	setFontSize();


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
			nppBeforeShutdown();
            break;

        case NPPN_BUFFERACTIVATED:
            if (!plugin.startupOrShutdown && !plugin.fileIsOpening) {
                plugin.getScintillaPointers();
                bufferActivated(nmhdr);
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
        case NPPN_FILERENAMED:
            fileRenamed(nmhdr);
			break;
        case NPPN_FILESAVED:
            fileSaved(nmhdr);
            break;
        case NPPN_GLOBALMODIFIED:
            modifyAll(nmhdr);
            break;

        case NPPN_DARKMODECHANGED:
            // Handle dark mode change - update any open dialogs
            if (virtualPanelWnd && IsWindow(virtualPanelWnd)) {
                npp(NPPM_DARKMODESUBCLASSANDTHEME, NPP::NppDarkMode::dmfHandleChange, virtualPanelWnd);
                SetWindowPos(virtualPanelWnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
                
                // Update tree colors
                HWND hTree = GetDlgItem(virtualPanelWnd, 1023);
                if (hTree) {
                    updateTreeColorsExternal(hTree);
                }
                
                // Resize the panel to match the new theme
                resizeVirtualPanel();
            }
            break;
        case NPPN_WORDSTYLESUPDATED:
            break;
        case NPPN_READONLYCHANGED:
			readOnlyChanged(nmhdr);
            break;
        case NPPN_NATIVELANGCHANGED:
			loadLocalization();
            loadMenus();
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
            bufferActivated(nmhdr);
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
		case Scintilla::Notification::SavePointLeft:
            plugin.getScintillaPointers(scnp);
            scnSavePointEvent(nmhdr->idFrom, false);
            break;
        case Scintilla::Notification::SavePointReached:
            plugin.getScintillaPointers(scnp);
            scnSavePointEvent(nmhdr->idFrom, true);
            break;
        }

    }

    plugin.bypassNotifications = false;

}


// This is rarely used, but a few Notepad++ commands call this routine as part of their processing

extern "C" __declspec(dllexport) LRESULT messageProc(UINT uInt, WPARAM wParam, LPARAM lParam) {
    /*LOG("UINT [{}]", uInt);
    LOG("WPARAM [{}]", wParam);
    LOG("LPARAM [{}]", lParam);*/
    return TRUE;
}