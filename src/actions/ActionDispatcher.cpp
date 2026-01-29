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

    uint32_t repeat = action.repeat == 0 ? 1 : action.repeat;
    for (uint32_t count = 0; count < repeat; ++count)
    {
        ActionStatus result = registry_.Dispatch(action, *this);
        if (!result.success)
        {
            return result;
        }
        if (action.delay_ms > 0)
        {
            delay(action.delay_ms);
        }
    }
    return ActionStatus::Ok();
}
