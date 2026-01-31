#include "ConfigService.h"

namespace ConfigService
{
ConfigLoader &Loader()
{
    static ConfigLoader loader;
    return loader;
}

bool Reload()
{
    return Loader().reloadConfig();
}

const ConfigRoot &GetConfig()
{
    return Loader().config();
}

const std::string &LastError()
{
    return Loader().lastError();
}
} // namespace ConfigService
