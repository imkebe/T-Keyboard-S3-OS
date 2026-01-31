#pragma once

#include <string>

#include "ConfigLoader.h"

namespace ConfigService
{
bool Reload();
const ConfigRoot &GetConfig();
const std::string &LastError();
ConfigLoader &Loader();
} // namespace ConfigService
