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
#include <filesystem>
#include "Translator.h"

#include <type_traits>

using ssize_t = std::make_signed_t<size_t>;



#define LOG(fmt, ...) { \
    auto logMessage = std::format(fmt, __VA_ARGS__); \
    logMessage = "***** VFOLDER LOG ***** : " + logMessage; \
    logMessage += "\n"; \
    OutputDebugStringA(logMessage.c_str()); \
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
    if (jsonFilePath.empty()) {
        return;
    }

    commonData.rootVFolder.vFolderSort();
    json vDataJson = commonData.rootVFolder;
    const std::string serialized = vDataJson.dump(4);

    const std::filesystem::path target(jsonFilePath);
    const std::wstring processSuffix = L"." + std::to_wstring(GetCurrentProcessId());
    const std::filesystem::path temporary(jsonFilePath + L".tmp" + processSuffix);
    const std::filesystem::path backup(jsonFilePath + L".bak");
    const std::filesystem::path backupTemporary(jsonFilePath + L".bak.tmp" + processSuffix);

    auto reportFailure = [](const std::wstring& message) {
        const std::wstring logMessage = L"VirtualFolders storage write failed: " + message + L"\n";
        OutputDebugStringW(logMessage.c_str());
    };

    std::error_code ec;
    std::filesystem::create_directories(target.parent_path(), ec);
    if (ec) {
        reportFailure(L"could not create the storage directory.");
        return;
    }

    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            reportFailure(L"could not open the temporary file.");
            return;
        }
        output.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
        output.flush();
        if (!output) {
            output.close();
            std::filesystem::remove(temporary, ec);
            reportFailure(L"could not write the complete temporary file.");
            return;
        }
    }

    {
        std::ifstream input(temporary, std::ios::binary);
        json verification = json::parse(input, nullptr, false);
        if (verification.is_discarded() || verification != vDataJson) {
            input.close();
            std::filesystem::remove(temporary, ec);
            reportFailure(L"temporary file verification failed.");
            return;
        }
    }

    if (std::filesystem::exists(target, ec) && !ec) {
        std::filesystem::copy_file(target, backupTemporary,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) {
            if (!MoveFileExW(backupTemporary.c_str(), backup.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                std::filesystem::remove(backupTemporary, ec);
            }
        }
    }

    if (!MoveFileExW(temporary.c_str(), target.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = GetLastError();
        std::filesystem::remove(temporary, ec);
        reportFailure(L"atomic replace failed with Windows error " + std::to_wstring(error) + L".");
    }
}

inline string folderToMinJson(VFolder folder) 
{

    auto setAllNames = [&](auto&& self, VFolder& f) -> void {
        f.name = "xxx " + to_string(f.getOrder());
        f.path = "xxx " + to_string(f.getOrder());
        for (auto& file : f.fileList) {
            file.name = "xxx " + to_string(file.getOrder());
            file.path = "xxx " + to_string(file.getOrder());
            file.backupFilePath = "xxx " + to_string(file.getOrder());
        }
        for (auto& sub : f.folderList) self(self, sub);
        };

    setAllNames(setAllNames, folder);


    
    folder.vFolderSort();
    json vDataJson = folder;

    std::string jsonStr = vDataJson.dump(0); // Serialize JSON to string
    

   /* vector<BYTE> compressedVector = safeCompress(vector<BYTE>(jsonStr.begin(), jsonStr.end()));
    string compressed = string(compressedVector.begin(), compressedVector.end());
    vector<BYTE> decommpressedVector = safeDecompress(compressedVector, jsonStr.size());
    string decompressed = string(decommpressedVector.begin(), decommpressedVector.end());

    std::string base64Str = base64_encode(compressed);

    if (jsonStr == decompressed) {
        LOG("compress-decompress is successfull");
    }
    */
    return jsonStr;
}


