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

#pragma once

#include "Framework/PluginFramework.h"
#include "Framework/ConfigFramework.h"
#include "Framework/UtilityFramework.h"
#include "model/VData.h"
#include <CommCtrl.h>
#include <format>
#include "Translator.h"



#define LOG(fmt, ...) { \
    auto msg = std::format(fmt, __VA_ARGS__); \
    msg = "***** VFOLDER LOG ***** : " + msg; \
    msg += "\n"; \
    OutputDebugStringA(msg.c_str()); \
}

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

    config<int>          fontSize = { "fontSize", 10 };
    //config_history       fontFamily = { "option2", {L"Consolas"} };
	config<std::wstring> fontFamily = { "fontFamily", L"Segoe UI" };
    //config<MyPreference> myPref  = { "MyPreference", MyPreference::Bacon };
    config<bool>         virtualFoldersTabSelected = { "VirtualFoldersTabSelected", false };

    std::vector<VFile> openFiles;
    VFolder rootVFolder;
    HWND hTree;
    UINT_PTR activeBufferID;
    std::map<UINT_PTR, bool> bufferStates;  // for save points
    std::map<UINT_PTR, int> bufferViewMap;  // to keep buffers view

	std::unique_ptr<Translator> nativeTranslator;
    std::unique_ptr<Translator> shortcutTranslator;
    std::unique_ptr<Translator> translator;


} commonData;

inline std::wstring jsonFilePath;

inline void writeJsonFile() {
    json vDataJson = commonData.rootVFolder;
    std::ofstream(jsonFilePath) << vDataJson.dump(4); // Write JSON to file
}

