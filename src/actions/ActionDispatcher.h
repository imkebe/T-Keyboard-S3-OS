#pragma once

#include <vector>

#include <Arduino.h>

#include "ActionStatus.h"
#include "../../libraries/T-Keyboard_S3_Drive/ConfigSchema.h"

class ActionRegistry;

class ActionDispatcher
{
public:
    explicit ActionDispatcher(const ActionRegistry &registry);

    ActionStatus DispatchKey(const KeyConfig &key) const;
    ActionStatus DispatchActions(const std::vector<ActionConfig> &actions) const;
    ActionStatus DispatchAction(const ActionConfig &action) const;

private:
    const ActionRegistry &registry_;
};
