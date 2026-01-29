#include "ActionDispatcher.h"

#include "ActionRegistry.h"

ActionDispatcher::ActionDispatcher(const ActionRegistry &registry) : registry_(registry)
{
}

void ActionDispatcher::DispatchKey(const KeyConfig &key) const
{
    if (!key.enabled)
    {
        return;
    }

    DispatchActions(key.actions);
}

void ActionDispatcher::DispatchActions(const std::vector<ActionConfig> &actions) const
{
    for (const auto &action : actions)
    {
        DispatchAction(action);
    }
}

void ActionDispatcher::DispatchAction(const ActionConfig &action) const
{
    if (!action.enabled)
    {
        return;
    }

    uint32_t repeat = action.repeat == 0 ? 1 : action.repeat;
    for (uint32_t count = 0; count < repeat; ++count)
    {
        registry_.Dispatch(action, *this);
        if (action.delay_ms > 0)
        {
            delay(action.delay_ms);
        }
    }
}
