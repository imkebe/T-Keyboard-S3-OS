/*
 * @Description: Config schema validation for T-Keyboard S3
 * @version: V1.0.0
 * @Author: None
 * @Date: 2024-04-02
 * @License: GPL 3.0
 */
#include "ConfigSchema.h"

const std::unordered_set<std::string> &ConfigRoot::AllowedActionTypes()
{
    static const std::unordered_set<std::string> types{
        "macro",
        "media",
        "keycode",
        "layer",
        "system",
        "custom",
    };
    return types;
}

ValidationResult ActionConfig::Validate() const
{
    ValidationResult result;
    if (id.empty())
    {
        result.errors.push_back("action.id is required");
    }
    if (type.empty())
    {
        result.errors.push_back("action.type is required");
    }
    else if (ConfigRoot::AllowedActionTypes().count(type) == 0)
    {
        result.errors.push_back("action.type must be one of the supported values");
    }
    return result;
}

ValidationResult KeyConfig::Validate() const
{
    ValidationResult result;
    if (id.empty())
    {
        result.errors.push_back("key.id is required");
    }
    return result;
}

ValidationResult ConfigRoot::Validate() const
{
    ValidationResult result;
    if (version != 1)
    {
        result.errors.push_back("config.version must be 1");
    }

    std::unordered_set<std::string> action_ids;
    for (const auto &action : actions)
    {
        auto action_result = action.Validate();
        result.errors.insert(result.errors.end(), action_result.errors.begin(), action_result.errors.end());
        if (!action.id.empty())
        {
            if (!action_ids.insert(action.id).second)
            {
                result.errors.push_back("action.id values must be unique");
            }
        }
    }

    std::unordered_set<std::string> key_ids;
    for (const auto &key : keys)
    {
        auto key_result = key.Validate();
        result.errors.insert(result.errors.end(), key_result.errors.begin(), key_result.errors.end());
        if (!key.id.empty())
        {
            if (!key_ids.insert(key.id).second)
            {
                result.errors.push_back("key.id values must be unique");
            }
        }
        if (!key.action_id.empty() && action_ids.count(key.action_id) == 0)
        {
            result.errors.push_back("key.action_id must reference a known action.id");
        }
    }

    return result;
}
