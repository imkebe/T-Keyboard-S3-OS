#include "ActionDispatcher.h"

#include "ActionRegistry.h"

ActionDispatcher::ActionDispatcher(const ActionRegistry &registry) : registry_(registry)
{
}

ActionStatus ActionDispatcher::DispatchKey(const KeyConfig &key) const
{
    if (!key.enabled)
    {
        return ActionStatus::Ok();
    }

    return DispatchActions(key.actions);
}

ActionStatus ActionDispatcher::DispatchActions(const std::vector<ActionConfig> &actions) const
{
    ActionStatus result = ActionStatus::Ok();
    for (const auto &action : actions)
    {
        result = DispatchAction(action);
        if (!result.success)
        {
            return result;
        }
    }
    return result;
}

ActionStatus ActionDispatcher::DispatchAction(const ActionConfig &action) const
{
    if (!action.enabled)
    {
        return ActionStatus::Ok();
    }

    auto validation = action.Validate();
    if (!validation.Ok())
    {
        for (const auto &error : validation.errors)
        {
            Serial.println(String("ActionDispatcher: skipping invalid action ") +
                           (action.id.empty() ? "<unnamed>" : action.id.c_str()) + " - " + error.c_str());
        }
        return ActionStatus::Ok();
    }

    uint32_t repeat = action.repeat == 0 ? 1 : action.repeat;
    if (repeat > ConfigLimits::kMaxActionRepeat)
    {
        Serial.println(String("ActionDispatcher: clamping repeat for action ") +
                       (action.id.empty() ? "<unnamed>" : action.id.c_str()) + " from " +
                       String(repeat) + " to " + String(ConfigLimits::kMaxActionRepeat));
        repeat = ConfigLimits::kMaxActionRepeat;
    }
    uint32_t delay_ms = action.delay_ms;
    if (delay_ms > ConfigLimits::kMaxActionDelayMs)
    {
        Serial.println(String("ActionDispatcher: clamping delay for action ") +
                       (action.id.empty() ? "<unnamed>" : action.id.c_str()) + " from " +
                       String(delay_ms) + " to " + String(ConfigLimits::kMaxActionDelayMs));
        delay_ms = ConfigLimits::kMaxActionDelayMs;
    }
    for (uint32_t count = 0; count < repeat; ++count)
    {
        ActionStatus result = registry_.Dispatch(action, *this);
        if (!result.success)
        {
            return result;
        }
        if (delay_ms > 0)
        {
            delay(delay_ms);
        }
    }
    return ActionStatus::Ok();
}
