#include "ActionRegistry.h"

#include "ActionDispatcher.h"
#include "HttpActionHandler.h"
#include "UsbHidMacroHandler.h"

#include <USBHIDKeyboard.h>

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
        return ActionStatus::Ok();
    });
    Register("ble_key", [](const ActionConfig &action, const ActionDispatcher &) {
        logAction("ble_key handler invoked", action);
        return ActionStatus::Ok();
    });
    Register("http_request", [](const ActionConfig &action, const ActionDispatcher &) {
        logAction("http_request handler invoked", action);
        return HandleHttpRequestAction(action);
    });
    Register("composite", [](const ActionConfig &action, const ActionDispatcher &dispatcher) {
        logAction("composite handler invoked", action);
        return dispatcher.DispatchActions(action.actions);
    });
    Register("macro", [this](const ActionConfig &action, const ActionDispatcher &) {
        logAction("macro handler invoked", action);
        if (!keyboard_)
        {
            Serial.println("ActionRegistry: macro handler missing USBHIDKeyboard");
            return ActionStatus::Failure(-1, "Missing USBHIDKeyboard");
        }
        UsbHidMacroHandler handler(*keyboard_);
        handler.Execute(action.payload);
        return ActionStatus::Ok();
    });
}

void ActionRegistry::SetUsbHidKeyboard(USBHIDKeyboard *keyboard)
{
    keyboard_ = keyboard;
}

void ActionRegistry::Register(const std::string &type, Handler handler)
{
    handlers_[type] = std::move(handler);
}

void ActionRegistry::RegisterDefault(Handler handler)
{
    default_handler_ = std::move(handler);
}

ActionStatus ActionRegistry::Dispatch(const ActionConfig &action, const ActionDispatcher &dispatcher) const
{
    auto it = handlers_.find(action.type);
    if (it == handlers_.end())
    {
        if (default_handler_)
        {
            return default_handler_(action, dispatcher);
        }
        return ActionStatus::Failure(-1, "Unknown action type");
    }

    return it->second(action, dispatcher);
}

ActionStatus ActionRegistry::DefaultUnknownHandler(const ActionConfig &action, const ActionDispatcher &)
{
    Serial.println(String("ActionRegistry: unknown action type (no-op): ") + action.type.c_str());
    return ActionStatus::Failure(-1, "Unknown action type");
}
