# NppCppMSVS: Visual Studio Project Template for a Notepad++ C++ Plugin

***Note: This is the readme for the template. In your project, replace this text with the readme for your own plugin.***

**NppCppMSVS: Visual Studio Project Template for a Notepad++ C++ Plugin** is a template for Microsoft Visual Studio which you can use to build C++ plugins for [Notepad++](https://github.com/notepad-plus-plus/notepad-plus-plus).

Like Notepad++, this software is released under the GNU General Public License (either version 3 of the License, or, at your option, any later version). Some files which do not contain any dependencies on Notepad++ are released under the [MIT License](https://www.opensource.org/licenses/MIT); see the individual source files for details.

This template uses [JSON for Modern C++](https://github.com/nlohmann/json) by Niels Lohmann (https://nlohmann.me), which is released under the [MIT License](https://www.opensource.org/licenses/MIT).

## Purpose

*This project is not a part of Notepad++ and is not endorsed by the author of Notepad++.*

This template includes framework code for making a Notepad++ plugin in C++, along with various utility functions and examples.

I have constructed it to give myself and others a useful starting point when developing a Notepad++ plugin in C++. 

It necessarily reflects my own idiosyncrasies as a programmer. I hope it may prove useful to others, and I welcome comments, suggestions and problem reports in the Issues; however, I make no apologies for its incorporation of my personal choices and styles — which might not meet with universal approval — and I do not promise to make changes to accommodate all plausible programming styles and use cases. So be it.

## Features

The template generates a working plugin that includes a menu command, a Settings dialog and an About dialog. That lets you start from something that already runs. The name you select for your project will be used as the plugin name (you can change it).

---

The [help file](https://coises.github.io/NppCppMSVS/help.htm) contains documentation regarding the files and functions included. You'll want to modify the copy of this file in your project to create your own help file; but you can still use the one linked here, or make a local copy of the original file, for reference.

---

The template includes the declarations and code needed to use the C++ interface to Scintilla, so you can write:
```
auto s = sci.GetLine(12);
```
instead of:
```
int currentEdit = 0;
SendMessage(plugin.nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&currentEdit));
auto scintillaHandle = currentEdit ? plugin.nppData._scintillaSecondHandle : plugin.nppData._scintillaMainHandle;
intptr_t n = SendMessage(scintillaHandle, SCI_LINELENGTH, 12, 0);
std::string s(n, 0);
SendMessage(scintillaHandle, SCI_GETLINE, 12, reinterpret_cast<LPARAM>(s.data()));
```

---

The template includes code to save and restore configuration data as a JSON file. Also included is a set of templates, classes and functions that assist in defining and using variables that are automatically backed by the JSON configuration store and can be easily used in a settings dialog; for example:
```
config<std::wstring> option1 = { "option 1", L"Favorites" };
config<bool>         option2 = { "option 2", false};
```
define a wide character string and a boolean that will be saved with the configuration, default to `L"Favorites"` and `false` if not found in the configuration, and can be displayed in an edit control and a check box by coding:
```
option1.put(hwndDlg, IDC_OPTION1);
option2.put(hwndDlg, IDC_OPTION2);
```
and read back from the dialog with:
```
option1.get(hwndDlg, IDC_OPTION1);
option2.get(hwndDlg, IDC_OPTION2);
```
while being used in most contexts as if they were plain `std::wstring` and `bool` variables.

## Installation

To use this template, place the zip file for the latest release in the **Visual Studio 2022\Templates\ProjectTemplates** folder within your **Documents** folder; you’ll probably want to rename it to something like **NppCppMSVS-*version*.zip**. The next time you start Visual Studio and choose **Create a new project**, you’ll find **Notepad++ C++ Plugin** available as a C++ project template.

When you create a project from this template, the initial Visual Studio project settings make it possible to test the plugin with an installed copy of Notepad++ (x86, x64 or both). After a successful build, the plugin dll and documentation files are copied to your Notepad++ plugin directory if Notepad++ is installed in the default location. *This only works if you set permissions on the directory to allow copying without elevated permissions.* You can modify the Post-Build Event in the Configuration Properties for the project if you want to deploy the plugin to a different copy of Notepad++ for testing, or disable the Post-Build Event if you want to copy it manually.

You will still have to set the debugging path manually. Right-click your project in the Solution Explorer in Visual Studio and select **Properties** from the menu. In the **Configuration Properties** section, select **Debugging**; then click **Command**, click the drop-down arrow at the right, select **Browse...**, then navigate to and select the copy of Notepad++ you will use for debugging. You will have to do this separately for 32-bit and 64-bit targets (pay attention to the selections at the top of the Properties dialog).
