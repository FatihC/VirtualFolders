#pragma once
#include <string>
#include <vector>
#include <optional>
#include <fstream>
#include <map>
#include <set>
#include <algorithm>
#include <windows.h>
#include "DateUtil.h"

using namespace std;


// Represents a single file entry in the session
class SessionFile
{
public:
    // File display name (can be "new X" for unsaved files)
    string filename;
    
    // Backup file path for unsaved files
    string backupFilePath;
    
    // Language type
    string language;
    
    // Encoding (-1 for default)
    int encoding;
    
    // File timestamps
    ULONGLONG originalFileLastModifTimestamp;
    ULONGLONG originalFileLastModifTimestampHigh;
    
    // View state
    int firstVisibleLine;
    int xOffset;
    int scrollWidth;
    int startPos;
    int endPos;
    int selMode;
    int offset;
    int wrapCount;
    
    // File properties
    bool userReadOnly;
    int tabColourId;
    bool RTL;
    bool tabPinned;
    
    // Map view properties
    int mapFirstVisibleDisplayLine;
    int mapFirstVisibleDocLine;
    int mapLastVisibleDocLine;
    int mapNbLine;
    int mapHigherPos;
    int mapWidth;
    int mapHeight;
    int mapKByteInDoc;
    int mapWrapIndentMode;
    bool mapIsWrap;
    
    // Helper methods
    bool isUnsavedFile() const {
        return !backupFilePath.empty() == 0;
    }
    
    string getDisplayName() const {
        if (isUnsavedFile()) {
            return filename;
        }
        // Extract filename from path
        size_t lastSlash = backupFilePath.find_last_of("/\\");
        return lastSlash != string::npos ? backupFilePath.substr(lastSlash + 1) : backupFilePath;
    }
};

// Represents a view (main or sub)
class SessionView
{
public:
    int activeIndex;
    vector<SessionFile> files;
    
    // Helper methods
    SessionFile* getActiveFile() {
        if (activeIndex >= 0 && activeIndex < static_cast<int>(files.size())) {
            return &files[activeIndex];
        }
        return nullptr;
    }
    
    const SessionFile* getActiveFile() const {
        if (activeIndex >= 0 && activeIndex < static_cast<int>(files.size())) {
            return &files[activeIndex];
        }
        return nullptr;
    }
};

// Represents the entire session
class Session
{
public:
    int activeView; // 0 for main view, 1 for sub view
    SessionView mainView;
    SessionView subView;
    
    // Helper methods
    SessionView* getActiveView() {
        return activeView == 0 ? &mainView : &subView;
    }
    
    const SessionView* getActiveView() const {
        return activeView == 0 ? &mainView : &subView;
    }
    
    vector<SessionFile> getAllFiles() const {
        vector<SessionFile> allFiles;
        allFiles.insert(allFiles.end(), mainView.files.begin(), mainView.files.end());
        allFiles.insert(allFiles.end(), subView.files.begin(), subView.files.end());
        return allFiles;
    }
    
    vector<SessionFile> getUnsavedFiles() const {
        vector<SessionFile> unsavedFiles;
        auto allFiles = getAllFiles();
        for (const auto& file : allFiles) {
            if (file.isUnsavedFile()) {
                unsavedFiles.push_back(file);
            }
        }
        return unsavedFiles;
    }
    
    vector<SessionFile> getSavedFiles() const {
        vector<SessionFile> savedFiles;
        auto allFiles = getAllFiles();
        for (const auto& file : allFiles) {
            if (!file.isUnsavedFile()) {
                savedFiles.push_back(file);
            }
        }
        return savedFiles;
    }
};

// XML parsing utility functions
inline bool parseBool(const string& value) {
    return value == "yes" || value == "true" || value == "1";
}

inline int parseInt(const string& value, int defaultValue = 0) {
    try {
        return stoi(value);
    } catch (...) {
        return defaultValue;
    }
}

inline ULONGLONG parseULongLong(const string& value, ULONGLONG defaultValue = 0) {
    try {
        return stoull(value);
    } catch (...) {
        return defaultValue;
    }
}

// Simple XML attribute parser
inline string getAttributeValue(const string& xml, const string& attrName, size_t& pos) {
    string searchStr = attrName + "=\"";
    size_t start = xml.find(searchStr, pos);
    if (start == string::npos) return "";
    
    start += searchStr.length();
    size_t end = xml.find("\"", start);
    if (end == string::npos) return "";
    
    //pos = end + 1; // position shouldn't be effected. because search starts at the start
    return xml.substr(start, end - start);
}

// Parse a single File element
inline SessionFile parseFileElement(const string& xml, size_t& pos) {
    SessionFile file;
    
    // Find the File tag
    size_t fileStart = xml.find("<File", pos);
    if (fileStart == string::npos) return file;
    
    size_t currentPos = fileStart;
    
    // Parse all attributes
    file.filename = getAttributeValue(xml, "filename", currentPos);
    file.backupFilePath = getAttributeValue(xml, "backupFilePath", currentPos);
    file.language = getAttributeValue(xml, "lang", currentPos);
    file.encoding = parseInt(getAttributeValue(xml, "encoding", currentPos), -1);
    
    file.originalFileLastModifTimestamp = parseULongLong(getAttributeValue(xml, "originalFileLastModifTimestamp", currentPos));
    file.originalFileLastModifTimestampHigh = parseULongLong(getAttributeValue(xml, "originalFileLastModifTimestampHigh", currentPos));
    
    file.firstVisibleLine = parseInt(getAttributeValue(xml, "firstVisibleLine", currentPos));
    file.xOffset = parseInt(getAttributeValue(xml, "xOffset", currentPos));
    file.scrollWidth = parseInt(getAttributeValue(xml, "scrollWidth", currentPos));
    file.startPos = parseInt(getAttributeValue(xml, "startPos", currentPos));
    file.endPos = parseInt(getAttributeValue(xml, "endPos", currentPos));
    file.selMode = parseInt(getAttributeValue(xml, "selMode", currentPos));
    file.offset = parseInt(getAttributeValue(xml, "offset", currentPos));
    file.wrapCount = parseInt(getAttributeValue(xml, "wrapCount", currentPos));
    
    file.userReadOnly = parseBool(getAttributeValue(xml, "userReadOnly", currentPos));
    file.tabColourId = parseInt(getAttributeValue(xml, "tabColourId", currentPos), -1);
    file.RTL = parseBool(getAttributeValue(xml, "RTL", currentPos));
    file.tabPinned = parseBool(getAttributeValue(xml, "tabPinned", currentPos));
    
    file.mapFirstVisibleDisplayLine = parseInt(getAttributeValue(xml, "mapFirstVisibleDisplayLine", currentPos), -1);
    file.mapFirstVisibleDocLine = parseInt(getAttributeValue(xml, "mapFirstVisibleDocLine", currentPos), -1);
    file.mapLastVisibleDocLine = parseInt(getAttributeValue(xml, "mapLastVisibleDocLine", currentPos), -1);
    file.mapNbLine = parseInt(getAttributeValue(xml, "mapNbLine", currentPos), -1);
    file.mapHigherPos = parseInt(getAttributeValue(xml, "mapHigherPos", currentPos), -1);
    file.mapWidth = parseInt(getAttributeValue(xml, "mapWidth", currentPos), -1);
    file.mapHeight = parseInt(getAttributeValue(xml, "mapHeight", currentPos), -1);
    file.mapKByteInDoc = parseInt(getAttributeValue(xml, "mapKByteInDoc", currentPos), 512);
    file.mapWrapIndentMode = parseInt(getAttributeValue(xml, "mapWrapIndentMode", currentPos), -1);
    file.mapIsWrap = parseBool(getAttributeValue(xml, "mapIsWrap", currentPos));
    
    // Find the end of the File tag
    size_t fileEnd = xml.find("/>", currentPos);
    if (fileEnd != string::npos) {
        pos = fileEnd + 2;
    }
    
    return file;
}

// Parse a view (mainView or subView)
inline SessionView parseView(const string& xml, const string& viewName, size_t& pos) {
    SessionView view;
    
    // Find the view tag
    string searchStr = "<" + viewName;
    size_t viewStart = xml.find(searchStr, pos);
    if (viewStart == string::npos) return view;
    
    // Parse activeIndex attribute
    size_t currentPos = viewStart;
    view.activeIndex = parseInt(getAttributeValue(xml, "activeIndex", currentPos));

    // Find the end of the view
    string endTag = "</" + viewName + ">";
    size_t viewEnd = xml.find(endTag, viewStart);
    if (viewEnd != string::npos) {
        pos = viewEnd + endTag.length();
    }


    
    // Find all File elements in this view
    size_t filePos = currentPos;
    while (true) {
        size_t fileStart = xml.find("<File", filePos);
        if (fileStart == string::npos) break;

        if (fileStart >= pos) {
            // If we reached the end of the view, break
            break;
        }
        
        SessionFile file = parseFileElement(xml, filePos);
        if (!file.filename.empty()) {
            view.files.push_back(file);
        }

        if (filePos >= pos) {
            // If we reached the end of the view, break
            break;
        }
    }
    
    
    
    return view;
}

// XML parsing functions
inline Session loadSessionFromXMLFile(const std::wstring& filePath) {
    Session session;
    
    std::ifstream file(filePath);
    if (!file.is_open()) return session;
    
    // Read the entire file
    string xmlContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    if (xmlContent.empty()) return session;

    
    // Find Session tag
    size_t sessionStart = xmlContent.find("<Session");
    if (sessionStart == string::npos) return session;
    
    // Parse activeView attribute
    size_t currentPos = sessionStart;
    session.activeView = parseInt(getAttributeValue(xmlContent, "activeView", currentPos));
    
    // Parse mainView
    session.mainView = parseView(xmlContent, "mainView", currentPos);
    
    // Parse subView
    session.subView = parseView(xmlContent, "subView", currentPos);
    
    return session;
}

inline bool saveSessionToXMLFile(const Session& session, const std::wstring& filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) return false;
    
    file << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n";
    file << "<NotepadPlus>\n";
    file << "    <Session activeView=\"" << session.activeView << "\">\n";
    
    // Write mainView
    file << "        <mainView activeIndex=\"" << session.mainView.activeIndex << "\">\n";
    for (const auto& sessionFile : session.mainView.files) {
        file << "            <File firstVisibleLine=\"" << sessionFile.firstVisibleLine 
             << "\" xOffset=\"" << sessionFile.xOffset 
             << "\" scrollWidth=\"" << sessionFile.scrollWidth 
             << "\" startPos=\"" << sessionFile.startPos 
             << "\" endPos=\"" << sessionFile.endPos 
             << "\" selMode=\"" << sessionFile.selMode 
             << "\" offset=\"" << sessionFile.offset 
             << "\" wrapCount=\"" << sessionFile.wrapCount 
             << "\" lang=\"" << sessionFile.language 
             << "\" encoding=\"" << sessionFile.encoding 
             << "\" userReadOnly=\"" << (sessionFile.userReadOnly ? "yes" : "no") 
             << "\" filename=\"" << sessionFile.filename 
             << "\" backupFilePath=\"" << sessionFile.backupFilePath 
             << "\" originalFileLastModifTimestamp=\"" << sessionFile.originalFileLastModifTimestamp 
             << "\" originalFileLastModifTimestampHigh=\"" << sessionFile.originalFileLastModifTimestampHigh 
             << "\" tabColourId=\"" << sessionFile.tabColourId 
             << "\" RTL=\"" << (sessionFile.RTL ? "yes" : "no") 
             << "\" tabPinned=\"" << (sessionFile.tabPinned ? "yes" : "no") 
             << "\" mapFirstVisibleDisplayLine=\"" << sessionFile.mapFirstVisibleDisplayLine 
             << "\" mapFirstVisibleDocLine=\"" << sessionFile.mapFirstVisibleDocLine 
             << "\" mapLastVisibleDocLine=\"" << sessionFile.mapLastVisibleDocLine 
             << "\" mapNbLine=\"" << sessionFile.mapNbLine 
             << "\" mapHigherPos=\"" << sessionFile.mapHigherPos 
             << "\" mapWidth=\"" << sessionFile.mapWidth 
             << "\" mapHeight=\"" << sessionFile.mapHeight 
             << "\" mapKByteInDoc=\"" << sessionFile.mapKByteInDoc 
             << "\" mapWrapIndentMode=\"" << sessionFile.mapWrapIndentMode 
             << "\" mapIsWrap=\"" << (sessionFile.mapIsWrap ? "yes" : "no") 
             << "\" />\n";
    }
    file << "        </mainView>\n";
    
    // Write subView
    file << "        <subView activeIndex=\"" << session.subView.activeIndex << "\" />\n";
    
    file << "    </Session>\n";
    file << "</NotepadPlus>\n";
    
    return true;
}

