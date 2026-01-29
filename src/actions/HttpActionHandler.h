#pragma once

#include <string>
#include <unordered_map>

#include "ActionStatus.h"
#include "../../libraries/T-Keyboard_S3_Drive/ConfigSchema.h"

struct HttpRequestDefinition
{
    std::string method = "GET";
    std::string url;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    uint32_t timeout_ms = 5000;
    uint32_t retries = 0;
};

ActionStatus HandleHttpRequestAction(const ActionConfig &action);
