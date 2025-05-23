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

#include "CommonData.h"

extern void updateStatusDialog();
extern void updateWatcherPanel();


void scnModified(const Scintilla::NotificationData* scnp) {
    using Scintilla::FlagSet;
    if (FlagSet(scnp->modificationType, Scintilla::ModificationFlags::InsertText)) ++data.insertsCounted;
    else if (FlagSet(scnp->modificationType, Scintilla::ModificationFlags::DeleteText)) ++data.deletesCounted;
    else return;
    updateStatusDialog();
    updateWatcherPanel();
}


void scnUpdateUI(const Scintilla::NotificationData* /* scnp */) {
}


void scnZoom(const Scintilla::NotificationData* /* scnp */) {
}


void bufferActivated() {
    updateWatcherPanel();
}


void fileClosed(const NMHDR* nmhdr) {
    if (!data.annoy) return;
    // If the file (buffer) is closed in one view but remains open in the other, or is moved from one view to the other,
    // we still get this notification. So we have to check to see if the buffer is still open in either view.
    auto position = npp(NPPM_GETPOSFROMBUFFERID, nmhdr->idFrom, 0);
    if (position == -1) /* file is no longer open in either view */ {
        MessageBox(plugin.nppData._nppHandle, L"You closed a file, didn't you?", L"VFolders", 0);
    }
}


void fileOpened(const NMHDR* nmhdr) {
    if (!data.annoy) return;
    UINT_PTR bufferID = nmhdr->idFrom;
    std::wstring filepath = getFilePath(bufferID);
    MessageBox(plugin.nppData._nppHandle, (L"You opened \"" + filepath + L"\", didn't you?").data(), L"VFolders", 0);
}


void modifyAll(const NMHDR* nmhdr) {
    // This message is sent once for each buffer ID in which text is modified; the same buffer could be visible in both views
    UINT_PTR bufferID = reinterpret_cast<UINT_PTR>(nmhdr->hwndFrom);
    intptr_t cdi1 = npp(NPPM_GETCURRENTDOCINDEX, 0, 0);
    intptr_t cdi2 = npp(NPPM_GETCURRENTDOCINDEX, 0, 1);
    bool visible1 = cdi1 < 0 ? false : bufferID == static_cast<UINT_PTR>(npp(NPPM_GETBUFFERIDFROMPOS, cdi1, 0));
    bool visible2 = cdi2 < 0 ? false : bufferID == static_cast<UINT_PTR>(npp(NPPM_GETBUFFERIDFROMPOS, cdi2, 1));
    if (visible1) {
        plugin.getScintillaPointers(plugin.nppData._scintillaMainHandle);
        // process changed visible main view
    }
    if (visible2) {
        plugin.getScintillaPointers(plugin.nppData._scintillaSecondHandle);
        // process changed visible secondary view
    }
}
