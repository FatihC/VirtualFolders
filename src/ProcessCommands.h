#pragma once

#include "model/VData.h"
#include "model/Session.h"


std::vector<VFile> listOpenFiles();
void printListOpenFiles();
VFile sessionFileToVFile(const SessionFile& sessionFile, int view);