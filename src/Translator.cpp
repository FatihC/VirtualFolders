#include "Translator.h"
#include <iostream>
#include <format>




#define LOG(fmt, ...) { \
    auto msg = std::format(fmt, __VA_ARGS__); \
    msg = "***** VFOLDER LOG ***** : " + msg; \
    msg += "\n"; \
    OutputDebugStringA(msg.c_str()); \
}


Translator::Translator(const std::string& filePath) {
    tinyxml2::XMLError err = doc.LoadFile(filePath.c_str());
    if (err != tinyxml2::XML_SUCCESS) {
        std::cerr << "Failed to load language file: " << filePath << "\n";
        LOG("Failed to load language file: [{}]", filePath);
        return;
    }
    loadFromDoc();
}

void Translator::loadFile(const std::string& filePath) {
    tinyxml2::XMLError err = doc.LoadFile(filePath.c_str());
    if (err != tinyxml2::XML_SUCCESS) {
        std::cerr << "Failed to load language file: " << filePath << "\n";
        LOG("Failed to load language file: [{}]", filePath);
        return;
    }
    loadFromDoc();
}

Translator::Translator(HINSTANCE hInst, int resId) {
    if (!loadXmlFromResource(hInst, resId, doc)) {
        std::cerr << "Failed to load language resource: " << resId << "\n";
        LOG("Failed to load language resource: [{}]", resId);
        return;
    }
    loadFromDoc();
}

void Translator::loadFromDoc() {
    if (!doc.RootElement()) return;

    tinyxml2::XMLElement* menu = doc.RootElement()->FirstChildElement("Menu");
    if (menu) loadItems(menu);

    tinyxml2::XMLElement* labels = doc.RootElement()->FirstChildElement("Labels");
    if (labels) loadItems(labels);

    tinyxml2::XMLElement* dialogs = doc.RootElement()->FirstChildElement("Dialogs");
    if (dialogs) loadDialogs(dialogs);

    tinyxml2::XMLElement* internalCommands = doc.RootElement()->FirstChildElement("InternalCommands");
    if (internalCommands)
    {
        loadShortcuts(internalCommands);
    }


	tinyxml2::XMLElement* nativeLanguage = doc.RootElement()->FirstChildElement("Native-Langue");
    if (!nativeLanguage) return;
    tinyxml2::XMLElement* nativeMenu = nativeLanguage->FirstChildElement("Menu");
    if (!nativeMenu) return;
    tinyxml2::XMLElement* nativeMain = nativeMenu->FirstChildElement("Main");
    if (!nativeMain) return;
    
    tinyxml2::XMLElement* nativeCommands = nativeMain->FirstChildElement("Commands");
    if (nativeCommands) {
        loadCommands(nativeCommands);
    }
    
}

void Translator::loadItems(tinyxml2::XMLElement* parent) {
    for (tinyxml2::XMLElement* item = parent->FirstChildElement("Item");
        item; item = item->NextSiblingElement("Item")) {
        const char* id = item->Attribute("id");
        const char* text = item->Attribute("text");
        if (id && text) texts[id] = text;
		LOG("Loaded item: id=[{}], text=[{}]", (id ? id : "null"), (text ? text : "null"));
    }
}

void Translator::loadCommands(tinyxml2::XMLElement* parent) {
    for (tinyxml2::XMLElement* item = parent->FirstChildElement("Item");
        item; item = item->NextSiblingElement("Item")) {
        const char* id = item->Attribute("id");
        const char* name = item->Attribute("name");
        if (id && name) commands[id] = name;
    }
}

void Translator::loadShortcuts(tinyxml2::XMLElement* parent) {
    for (tinyxml2::XMLElement* item = parent->FirstChildElement("Shortcut");
        item; item = item->NextSiblingElement("Shortcut")) {
        const char* id = item->Attribute("id");
        const char* ctrl = item->Attribute("Ctrl");
        const char* alt = item->Attribute("Alt");
        const char* shift = item->Attribute("Shift");
        const char* key = item->Attribute("Key");

        if (!id) continue;
        if (ctrl) {
			commands[id] += (std::string(ctrl) == "yes" ? "Ctrl+" : "");
        }
		if (alt) {
            commands[id] += (std::string(alt) == "yes" ? "Alt+" : "");
		}
        if (shift) {
			commands[id] += (std::string(shift) == "yes" ? "Shift+" : "");
		}
		commands[id] += fromWchar(vkToKeyName((key ? std::stoi(key) : 0)).c_str());
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

wstring Translator::getTextW(const std::string& id) const {
    return toWstring(getText(id));
}

std::string Translator::getCommand(const std::string& id) const {
    auto it = commands.find(id);
    if (it != commands.end()) return it->second;
    return ""; // fallback
}

wchar_t* Translator::getCommandW(const std::string& id) const {
    return toWchar(getCommand(id));
}

std::string Translator::getShortcut(const std::string& id) const {
    auto it = shortcuts.find(id);
    if (it != shortcuts.end()) return it->second;
    return ""; // fallback
}

wchar_t* Translator::getShortcutW(const std::string& id) const {
    return toWchar(getShortcut(id));
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

void Translator::reload() {
    texts.clear();
}
