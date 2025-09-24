#pragma once
#include "tinyxml2/tinyxml2.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include "Util.h"


class Translator {
public:
    // Load from external file
    Translator(const std::string& filePath);

    // Load from resource
    Translator(HINSTANCE hInst, int resId);

	void loadFile(const std::string& filePath);

    // Get translated text by ID
    std::string getText(const std::string& id) const;
    wchar_t* getTextW(const std::string& id) const;
    
    std::string getCommand(const std::string& id) const;
    wchar_t* getCommandW(const std::string& id) const;

    std::string getShortcut(const std::string& id) const;
    wchar_t* getShortcutW(const std::string& id) const;


private:
    tinyxml2::XMLDocument doc;
    std::unordered_map<std::string, std::string> texts;
    std::unordered_map<std::string, std::string> commands;
    std::unordered_map<std::string, std::string> shortcuts;

    void loadFromDoc();
    void loadItems(tinyxml2::XMLElement* parent);
    void loadDialogs(tinyxml2::XMLElement* parent);

    void loadCommands(tinyxml2::XMLElement* parent);
    void loadShortcuts(tinyxml2::XMLElement* parent);

    static bool loadXmlFromResource(HINSTANCE hInst, int resId, tinyxml2::XMLDocument& doc);

public:
    std::wstring vkToKeyName(UINT vk)
    {
        UINT scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC) << 16;

        // Special handling for some keys (PageUp/Down, arrows, etc.)
        if (vk == VK_LEFT || vk == VK_UP || vk == VK_RIGHT || vk == VK_DOWN ||
            vk == VK_PRIOR || vk == VK_NEXT || vk == VK_END || vk == VK_HOME ||
            vk == VK_INSERT || vk == VK_DELETE)
        {
            scanCode |= 0x01000000; // set extended-key flag
        }

        wchar_t keyName[64];
        int len = GetKeyNameTextW((LONG)scanCode, keyName, 64);
        if (len > 0)
            return std::wstring(keyName, len);

        return L"Unknown";
    }
};
