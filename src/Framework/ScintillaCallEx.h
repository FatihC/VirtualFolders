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


// ScintillaCallEx.cpp and ScintillaCallEx.h patch exceptions raised by the ScintillaCall interface
// so that they are derived from std::exception, which can improve exception handling in some hosts.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include "../Host/ScintillaTypes.h"
#include "../Host/ScintillaMessages.h"
#include "../Host/ScintillaStructures.h"

#define Failure __unused__Failure

#include "../Host/ScintillaCall.h"

#undef Failure

#include <charconv>

namespace Scintilla {

struct Failure : std::exception {
    Scintilla::Status status;
    explicit Failure(Scintilla::Status status) noexcept : status(status) {}
    virtual const char* what() const noexcept override {
        static char text[37] = "Scintilla error; status code ";
        static auto tcr = std::to_chars(text + 29, text + 36, static_cast<int>(status));
        if (tcr.ec == std::errc()) *tcr.ptr = 0;
        else strcpy(text + 29, "unknown");
        return text;
    }
};

}
