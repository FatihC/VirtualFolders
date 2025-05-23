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

void listOpenFiles() {
    Scintilla::EndOfLine eolMode = sci.EOLMode();
    std::wstring eol = eolMode == Scintilla::EndOfLine::Cr ? L"\r"
                     : eolMode == Scintilla::EndOfLine::Lf ? L"\n"
                     : L"\r\n";
    std::wstring filenames = data.heading.get() + eol;
    for (int view = 0; view < 2; ++view) if (npp(NPPM_GETCURRENTDOCINDEX, 0, view)) {
        size_t n = npp(NPPM_GETNBOPENFILES, 0, view + 1);
        for (size_t i = 0; i < n; ++i) filenames += getFilePath(npp(NPPM_GETBUFFERIDFROMPOS, i, view)) + eol;
    }
    sci.InsertText(-1, fromWide(filenames).data());
}