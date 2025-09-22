#include "Translator.h"
#include <iostream>



Translator::Translator(const std::string& filePath) {
    tinyxml2::XMLError err = doc.LoadFile(filePath.c_str());
    if (err != tinyxml2::XML_SUCCESS) {
        std::cerr << "Failed to load language file: " << filePath << "\n";
        return;
    }
    loadFromDoc();
}

Translator::Translator(HINSTANCE hInst, int resId) {
    if (!loadXmlFromResource(hInst, resId, doc)) {
        std::cerr << "Failed to load language resource: " << resId << "\n";
        return;
    }
    loadFromDoc();
}

void Translator::loadFromDoc() {
    if (!doc.RootElement()) return;

    tinyxml2::XMLElement* menu = doc.RootElement()->FirstChildElement("Menu");
    if (menu) loadItems(menu);

    tinyxml2::XMLElement* dialogs = doc.RootElement()->FirstChildElement("Dialogs");
    if (dialogs) loadDialogs(dialogs);
}

void Translator::loadItems(tinyxml2::XMLElement* parent) {
    for (tinyxml2::XMLElement* item = parent->FirstChildElement("Item");
        item; item = item->NextSiblingElement("Item")) {
        const char* id = item->Attribute("id");
        const char* text = item->Attribute("text");
        if (id && text) texts[id] = text;
    }
}

void Translator::loadDialogs(tinyxml2::XMLElement* parent) {
    for (tinyxml2::XMLElement* dialog = parent->FirstChildElement("Dialog");
        dialog; dialog = dialog->NextSiblingElement("Dialog")) {
        const char* dlgId = dialog->Attribute("id");
        const char* title = dialog->Attribute("title");
        if (dlgId && title) texts[std::string(dlgId) + ".title"] = title;

        for (tinyxml2::XMLElement* text = dialog->FirstChildElement("Text");
            text; text = text->NextSiblingElement("Text")) {
            const char* textId = text->Attribute("id");
            const char* value = text->GetText();
            if (textId && value) texts[std::string(dlgId) + "." + textId] = value;
        }
    }
}

std::string Translator::getText(const std::string& id) const {
    auto it = texts.find(id);
    if (it != texts.end()) return it->second;
    return ""; // fallback
}

wchar_t* Translator::getTextW(const std::string& id) const {
    return toWchar(getText(id));
}

bool Translator::loadXmlFromResource(HINSTANCE hInst, int resId, tinyxml2::XMLDocument& doc) {
    HRSRC hRes = FindResource(hInst, MAKEINTRESOURCE(resId), RT_RCDATA);
    if (!hRes) return false;

    HGLOBAL hData = LoadResource(hInst, hRes);
    if (!hData) return false;

    DWORD size = SizeofResource(hInst, hRes);
    const char* data = static_cast<const char*>(LockResource(hData));

    if (!data || size == 0) return false;

    return (doc.Parse(data, size) == tinyxml2::XML_SUCCESS);
}
