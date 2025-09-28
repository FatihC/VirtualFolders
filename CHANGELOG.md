# NppCppMSVS: Visual Studio Project Template for a Notepad++ C++ Plugin -- Releases

***Note: This is the changelog for the template. In your project, replace this text with the changelog for your own plugin.***

## Version 1.4 -- September 19th, 2025

* Corrected errors in fromWide and toWide functions that could cause them to fail with very large strings.
* Updated files in the Host directory to Notepad++ 8.8.5 and Scintilla 5.5.7.
* Updated Nlohmann JSON for Modern C++ to version 3.12.0.

## Version 1.3 -- May 4th, 2025

* Updated files in the Host directory to Notepad++ 8.8.1 and Scintilla 5.5.6.

## Version 1.2 -- April 4th, 2025

* Added a PowerShell script, ZipForRelease.ps1, to make zip files for x86 and x64 releases. This script is in the root folder, and it puts the zip files in the Releases folder under the root folder. It also adds hashes for the zip files to Releases\Hashes.txt; these are needed if you want to add the plugin to the Notepad++ Plugins List.

## Version 1.1 -- March 24th, 2025

* Updated Scintilla files in the Host directory to Scintilla 5.5.5 (will be in Notepad++ v8.7.9).

* Changed `npp` template in Framework\\UtilityFramework.h to support enum arguments without casting.

* Changed the name that shows in the Visual Studio project templates list to **C++ Plugin for Notepad++**.

* Corrected installation instructions.

## Version 1.0 -- March 5th, 2025

* First release.