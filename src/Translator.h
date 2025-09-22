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

    // Get translated text by ID
    std::string getText(const std::string& id) const;
    wchar_t* getTextW(const std::string& id) const;

private:
    tinyxml2::XMLDocument doc;
    std::unordered_map<std::string, std::string> texts;

    void loadFromDoc();
    void loadItems(tinyxml2::XMLElement* parent);
    void loadDialogs(tinyxml2::XMLElement* parent);

    static bool loadXmlFromResource(HINSTANCE hInst, int resId, tinyxml2::XMLDocument& doc);
};
