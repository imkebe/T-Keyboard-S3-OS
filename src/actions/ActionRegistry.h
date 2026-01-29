#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include <Arduino.h>

#include "../../libraries/T-Keyboard_S3_Drive/ConfigSchema.h"

class ActionDispatcher;

class ActionRegistry
{
public:
    using Handler = std::function<void(const ActionConfig &, const ActionDispatcher &)>;

    ActionRegistry();

    void Register(const std::string &type, Handler handler);
    void RegisterDefault(Handler handler);
    void Dispatch(const ActionConfig &action, const ActionDispatcher &dispatcher) const;

private:
    static void DefaultUnknownHandler(const ActionConfig &action, const ActionDispatcher &dispatcher);

    std::unordered_map<std::string, Handler> handlers_{};
    Handler default_handler_{};
};
