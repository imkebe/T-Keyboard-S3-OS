#include "ActionRegistry.h"

#include "ActionDispatcher.h"

namespace
{
void logAction(const char *label, const ActionConfig &action)
{
    Serial.println(String("ActionRegistry: ") + label + " type=" + action.type.c_str() + " payload=" + action.payload.c_str());
}
} // namespace

ActionRegistry::ActionRegistry()
{
    default_handler_ = DefaultUnknownHandler;

    Register("hid_key", [](const ActionConfig &action, const ActionDispatcher &) {
        logAction("hid_key handler invoked", action);
    });
    Register("ble_key", [](const ActionConfig &action, const ActionDispatcher &) {
        logAction("ble_key handler invoked", action);
    });
    Register("http_request", [](const ActionConfig &action, const ActionDispatcher &) {
        logAction("http_request handler invoked", action);
    });
    Register("composite", [](const ActionConfig &action, const ActionDispatcher &dispatcher) {
        logAction("composite handler invoked", action);
        dispatcher.DispatchActions(action.actions);
    });
}

void ActionRegistry::Register(const std::string &type, Handler handler)
{
    handlers_[type] = std::move(handler);
}

void ActionRegistry::RegisterDefault(Handler handler)
{
    default_handler_ = std::move(handler);
}

void ActionRegistry::Dispatch(const ActionConfig &action, const ActionDispatcher &dispatcher) const
{
    auto it = handlers_.find(action.type);
    if (it == handlers_.end())
    {
        if (default_handler_)
        {
            default_handler_(action, dispatcher);
        }
        return;
    }

    it->second(action, dispatcher);
}

void ActionRegistry::DefaultUnknownHandler(const ActionConfig &action, const ActionDispatcher &)
{
    Serial.println(String("ActionRegistry: unknown action type (no-op): ") + action.type.c_str());
}
