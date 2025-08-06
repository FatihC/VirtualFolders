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

#pragma once

#include "Framework/PluginFramework.h"
#include "Framework/ConfigFramework.h"
#include "Framework/UtilityFramework.h"


// Define enumerations for use with config, and tell the JSON package how represent them in the configuration file

enum class MyPreference {Bacon, IceCream, Pizza};
NLOHMANN_JSON_SERIALIZE_ENUM(MyPreference, {
    {MyPreference::Bacon   , "Bacon"},
    {MyPreference::IceCream, "Ice Cream"},
    {MyPreference::Pizza   , "Pizza"}
})


// Common data structure

inline struct CommonData {

    bool isNppReady = false;
    int insertsCounted = 0;
    int deletesCounted = 0;

    // Data to be saved in the configuration file

    config<int>          option1 = { "option1", 10 };
    config_history       option2 = { "option2", {L"This remembers history."} };
    config<std::wstring> heading = { "Heading", L"List of open files:" };
    config<bool>         annoy   = { "Annoy"  , false };
    config<MyPreference> myPref  = { "MyPreference", MyPreference::Bacon };

} commonData;
