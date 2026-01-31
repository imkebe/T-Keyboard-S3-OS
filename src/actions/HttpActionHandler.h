#pragma once

#include "ActionStatus.h"
#include "../../libraries/T-Keyboard_S3_Drive/ConfigSchema.h"

using HttpRequestDefinition = ActionConfig::HttpRequest;

ActionStatus HandleHttpRequestAction(const ActionConfig &action);
