/*
 * @Description: Config schema data model for T-Keyboard S3
 * @version: V1.0.0
 * @Author: None
 * @Date: 2024-04-02
 * @License: GPL 3.0
 */
#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

struct ValidationResult
{
    std::vector<std::string> errors;

    bool Ok() const
    {
        return errors.empty();
    }
};

struct ActionConfig
{
    std::string id;
    std::string type;
    std::string payload;
    uint32_t delay_ms = 0;
    uint32_t repeat = 1;
    bool enabled = true;
    std::vector<ActionConfig> actions;

    ValidationResult Validate() const;
};

struct KeyConfig
{
    std::string id;
    std::string label;
    std::string icon;
    std::string action_id;
    std::vector<ActionConfig> actions;
    uint8_t key_index = 0;
    bool enabled = true;

    ValidationResult Validate() const;
};

struct ProfileConfig
{
    std::string id;
    std::string label;
    std::vector<KeyConfig> keys;
    std::vector<ActionConfig> actions;

    ValidationResult Validate() const;
};

struct ConfigRoot
{
    uint32_t version = 1;
    uint32_t debounce_ms = 30;
    std::string active_profile;
    std::vector<KeyConfig> keys;
    std::vector<ActionConfig> actions;
    std::vector<ProfileConfig> profiles;

    ValidationResult Validate() const;

    static const std::unordered_set<std::string> &AllowedActionTypes();

    const ProfileConfig *FindProfile(const std::string &profile_id) const;
    const ProfileConfig *ActiveProfile() const;
};
