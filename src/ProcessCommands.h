#pragma once

#include "model/VData.h"


std::vector<VFile> listOpenFiles();
void setVFileInfo(VFile& vFile);
void printListOpenFiles();