#pragma once

#include <vector>

#include <Arduino.h>

#include "../../libraries/T-Keyboard_S3_Drive/ConfigSchema.h"

class ActionRegistry;

class ActionDispatcher
{
public:
    explicit ActionDispatcher(const ActionRegistry &registry);

    void DispatchKey(const KeyConfig &key) const;
    void DispatchActions(const std::vector<ActionConfig> &actions) const;
    void DispatchAction(const ActionConfig &action) const;

private:
    const ActionRegistry &registry_;
};
