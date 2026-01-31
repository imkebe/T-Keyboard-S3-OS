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
        "hid_key",
        "ble_key",
        "http_request",
        "composite",
        "macro",
        "media",
        "keycode",
        "layer",
        "system",
        "custom",
        "profile_switch",
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
        result.errors.push_back("action.type must be one of the supported values (value=" + type + ")");
    }
    if (delay_ms > ConfigLimits::kMaxActionDelayMs)
    {
        result.errors.push_back("action.delay_ms exceeds max (id=" + id + ", value=" +
                                std::to_string(delay_ms) + ", max=" +
                                std::to_string(ConfigLimits::kMaxActionDelayMs) + ")");
    }
    if (repeat > ConfigLimits::kMaxActionRepeat)
    {
        result.errors.push_back("action.repeat exceeds max (id=" + id + ", value=" + std::to_string(repeat) +
                                ", max=" + std::to_string(ConfigLimits::kMaxActionRepeat) + ")");
    }
    if (type == "macro" && payload.size() > ConfigLimits::kMaxMacroPayloadLength)
    {
        result.errors.push_back("action.payload macro length exceeds max (id=" + id + ", length=" +
                                std::to_string(payload.size()) + ", max=" +
                                std::to_string(ConfigLimits::kMaxMacroPayloadLength) + ")");
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

ValidationResult ProfileConfig::Validate() const
{
    ValidationResult result;

    std::unordered_set<std::string> action_ids;
    for (const auto &action : actions)
    {
        auto action_result = action.Validate();
        result.errors.insert(result.errors.end(), action_result.errors.begin(), action_result.errors.end());
        if (!action.id.empty())
        {
            if (!action_ids.insert(action.id).second)
            {
                result.errors.push_back("action.id values must be unique (duplicate=" + action.id + ")");
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
                result.errors.push_back("key.id values must be unique (duplicate=" + key.id + ")");
            }
        }
        if (!key.action_id.empty() && action_ids.count(key.action_id) == 0)
        {
            result.errors.push_back("key.action_id must reference a known action.id (key=" + key.id +
                                    ", action_id=" + key.action_id + ")");
        }
    }

    return result;
}

ValidationResult ConfigRoot::Validate() const
{
    ValidationResult result;
    if (version != 1)
    {
        result.errors.push_back("config.version must be 1 (value=" + std::to_string(version) + ")");
    }

    if (profiles.empty())
    {
        ProfileConfig base_profile;
        base_profile.keys = keys;
        base_profile.actions = actions;
        auto base_result = base_profile.Validate();
        result.errors.insert(result.errors.end(), base_result.errors.begin(), base_result.errors.end());
        if (!active_profile.empty())
        {
            result.errors.push_back("config.active_profile requires profiles");
        }
        return result;
    }

    std::unordered_set<std::string> profile_ids;
    for (const auto &profile : profiles)
    {
        if (profile.id.empty())
        {
            result.errors.push_back("profile.id is required");
        }
        else if (!profile_ids.insert(profile.id).second)
        {
            result.errors.push_back("profile.id values must be unique (duplicate=" + profile.id + ")");
        }

        auto profile_result = profile.Validate();
        result.errors.insert(result.errors.end(), profile_result.errors.begin(), profile_result.errors.end());
    }

    if (!active_profile.empty() && !FindProfile(active_profile))
    {
        result.errors.push_back("config.active_profile must reference an existing profile.id (value=" +
                                active_profile + ")");
    }

    return result;
}

const ProfileConfig *ConfigRoot::FindProfile(const std::string &profile_id) const
{
    for (const auto &profile : profiles)
    {
        if (profile.id == profile_id)
        {
            return &profile;
        }
    }
    return nullptr;
}

const ProfileConfig *ConfigRoot::ActiveProfile() const
{
    if (profiles.empty())
    {
        return nullptr;
    }

    if (!active_profile.empty())
    {
        if (const auto *profile = FindProfile(active_profile))
        {
            return profile;
        }
    }

    return &profiles.front();
}
