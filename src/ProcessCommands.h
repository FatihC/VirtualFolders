#pragma once

#include "model/VData.h"
#include "model/Session.h"


std::vector<VFile> listOpenFiles();
optional<VFile> sessionFileToVFile(const SessionFile& sessionFile, int view);