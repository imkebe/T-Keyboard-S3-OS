#include "ConfigLoader.h"

#include <cstdlib>
#include <sstream>

ConfigLoader::ConfigLoader() = default;

const ConfigRoot &ConfigLoader::config() const
{
    return config_;
}

const std::string &ConfigLoader::lastError() const
{
    return last_error_;
}

bool ConfigLoader::SetActiveProfile(const std::string &profile_id)
{
    if (config_.profiles.empty())
    {
        return false;
    }
    if (!config_.FindProfile(profile_id))
    {
        return false;
    }
    config_.active_profile = profile_id;
    return true;
}

bool ConfigLoader::reloadConfig()
{
    std::vector<std::string> errors;
    ConfigRoot loaded;

    if (!loadFromSd(loaded, errors) && !loadFromInternalFs(loaded, errors))
    {
        last_error_ = "Failed to load /config.yaml from SD or internal FS";
        errors.push_back(last_error_);
        logErrors(errors);
        return false;
    }

    auto validation = loaded.Validate();
    if (!validation.Ok())
    {
        last_error_ = "Config validation failed";
        errors.insert(errors.end(), validation.errors.begin(), validation.errors.end());
        logErrors(errors);
        return false;
    }

    config_ = loaded;
    last_error_.clear();
    return true;
}

bool ConfigLoader::loadFromSd(ConfigRoot &out_config, std::vector<std::string> &errors)
{
    if (!SD.begin())
    {
        errors.push_back("SD.begin failed while looking for /config.yaml");
        return false;
    }

    if (loadFromFile(SD, "/config.yaml", out_config, errors))
    {
        return true;
    }

    errors.push_back("/config.yaml not found on SD");
    return false;
}

bool ConfigLoader::loadFromInternalFs(ConfigRoot &out_config, std::vector<std::string> &errors)
{
    if (!SPIFFS.begin())
    {
        errors.push_back("SPIFFS.begin failed while looking for /config.yaml");
        return false;
    }

    if (loadFromFile(SPIFFS, "/config.yaml", out_config, errors))
    {
        return true;
    }

    errors.push_back("/config.yaml not found on internal FS");
    return false;
}

bool ConfigLoader::loadFromFile(fs::FS &fs, const char *path, ConfigRoot &out_config, std::vector<std::string> &errors)
{
    File file = fs.open(path, "r");
    if (!file)
    {
        return false;
    }

    ParseState state;
    while (file.available())
    {
        String line = file.readStringUntil('\n');
        std::string line_str = line.c_str();
        if (!parseLine(line_str, state, out_config, errors))
        {
            return false;
        }
    }

    finalizeParse(state, out_config);
    return true;
}

bool ConfigLoader::parseLine(const std::string &line, ParseState &state, ConfigRoot &out_config, std::vector<std::string> &errors)
{
    const size_t indent = line.find_first_not_of(" \t");
    std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed.front() == '#')
    {
        return true;
    }

    if (state.section == Section::KeyActions && indent <= state.key_action_indent)
    {
        finalizeKeyAction(state);
        state.section = state.key_action_parent;
        state.key_action_parent = Section::None;
        state.key_action_indent = 0;
    }
    if ((state.section == Section::ActionSteps || state.section == Section::ActionHeaders) &&
        indent <= state.action_detail_indent)
    {
        state.section = state.action_detail_parent;
        state.action_detail_parent = Section::None;
        state.action_detail_indent = 0;
    }

    if (indent == 0 && trimmed == "config:")
    {
        finalizeParse(state, out_config);
        state.section = Section::Config;
        return true;
    }
    if (indent == 0 && trimmed == "keys:")
    {
        finalizeParse(state, out_config);
        state.section = Section::Keys;
        return true;
    }
    if (indent == 0 && trimmed == "actions:")
    {
        finalizeParse(state, out_config);
        state.section = Section::Actions;
        return true;
    }
    if (indent == 0 && trimmed == "profiles:")
    {
        finalizeParse(state, out_config);
        state.section = Section::Profiles;
        return true;
    }
    if (indent >= 4 && trimmed == "keys:" &&
        (state.section == Section::Profiles || state.section == Section::ProfileActions || state.section == Section::ProfileKeys))
    {
        if (!state.has_current_profile)
        {
            errors.push_back("profiles.keys requires an active profile");
            return false;
        }
        finalizeEntry(state, out_config);
        state.section = Section::ProfileKeys;
        return true;
    }
    if (indent >= 4 && trimmed == "actions:" &&
        (state.section == Section::Profiles || state.section == Section::ProfileActions || state.section == Section::ProfileKeys))
    {
        if (!state.has_current_profile)
        {
            errors.push_back("profiles.actions requires an active profile");
            return false;
        }
        finalizeEntry(state, out_config);
        state.section = Section::ProfileActions;
        return true;
    }
    if (trimmed == "actions:" && (state.section == Section::Keys || state.section == Section::ProfileKeys))
    {
        if (!state.has_current_key)
        {
            errors.push_back("key.actions requires an active key");
            return false;
        }
        state.key_action_parent = state.section;
        state.section = Section::KeyActions;
        state.key_action_indent = static_cast<uint8_t>(indent);
        state.has_current_key_action = false;
        state.current_key_action = ActionConfig{};
        return true;
    }
    if (trimmed == "steps:" &&
        (state.section == Section::Actions || state.section == Section::ProfileActions || state.section == Section::KeyActions))
    {
        if (state.section == Section::KeyActions && !state.has_current_key_action)
        {
            errors.push_back("action.steps requires an active key action");
            return false;
        }
        if (state.section != Section::KeyActions && !state.has_current_action)
        {
            errors.push_back("action.steps requires an active action");
            return false;
        }
        state.action_detail_parent = state.section;
        state.action_detail_indent = static_cast<uint8_t>(indent);
        state.section = Section::ActionSteps;
        return true;
    }
    if (trimmed == "headers:" &&
        (state.section == Section::Actions || state.section == Section::ProfileActions || state.section == Section::KeyActions))
    {
        if (state.section == Section::KeyActions && !state.has_current_key_action)
        {
            errors.push_back("action.headers requires an active key action");
            return false;
        }
        if (state.section != Section::KeyActions && !state.has_current_action)
        {
            errors.push_back("action.headers requires an active action");
            return false;
        }
        state.action_detail_parent = state.section;
        state.action_detail_indent = static_cast<uint8_t>(indent);
        state.section = Section::ActionHeaders;
        return true;
    }

    auto actionTargetForDetail = [&]() -> ActionConfig * {
        if (state.action_detail_parent == Section::KeyActions)
        {
            return &state.current_key_action;
        }
        return &state.current_action;
    };

    if (state.section == Section::ActionSteps)
    {
        if (trimmed.rfind("-", 0) != 0)
        {
            errors.push_back("action.steps entries must be list items");
            return false;
        }
        std::string remainder = trim(trimmed.substr(1));
        if (remainder.empty())
        {
            errors.push_back("action.steps entries must include a field");
            return false;
        }
        auto pos = remainder.find(':');
        if (pos == std::string::npos)
        {
            errors.push_back("Malformed action step: " + remainder);
            return false;
        }
        std::string field = trim(remainder.substr(0, pos));
        std::string value = stripQuotes(trim(remainder.substr(pos + 1)));
        ActionConfig *target = actionTargetForDetail();
        ActionConfig::MacroStep step;
        if (field == "press")
        {
            step.type = ActionConfig::MacroStep::Type::Press;
            step.value = value;
        }
        else if (field == "release")
        {
            step.type = ActionConfig::MacroStep::Type::Release;
            step.value = value;
        }
        else if (field == "text")
        {
            step.type = ActionConfig::MacroStep::Type::Text;
            step.value = value;
        }
        else if (field == "delay_ms" || field == "delay")
        {
            uint32_t delay_ms = 0;
            if (!parseUInt32(value, delay_ms))
            {
                errors.push_back("action.steps.delay_ms must be an integer");
                return false;
            }
            step.type = ActionConfig::MacroStep::Type::Delay;
            step.delay_ms = delay_ms;
        }
        else
        {
            errors.push_back("Unknown action step: " + field);
            return false;
        }
        target->macro_steps.push_back(step);
        return true;
    }

    if (state.section == Section::ActionHeaders)
    {
        if (trimmed.rfind("-", 0) != 0)
        {
            errors.push_back("action.headers entries must be list items");
            return false;
        }
        std::string remainder = trim(trimmed.substr(1));
        if (remainder.empty())
        {
            errors.push_back("action.headers entries must include a field");
            return false;
        }
        auto pos = remainder.find(':');
        if (pos == std::string::npos)
        {
            errors.push_back("Malformed header entry: " + remainder);
            return false;
        }
        std::string field = trim(remainder.substr(0, pos));
        std::string value = stripQuotes(trim(remainder.substr(pos + 1)));
        ActionConfig *target = actionTargetForDetail();
        target->http_request.headers[field] = value;
        target->has_http_request = true;
        return true;
    }

    auto assignField = [&](const std::string &field, const std::string &value) {
        if (state.section == Section::Config || state.section == Section::None)
        {
            if (field == "version")
            {
                uint32_t version = 0;
                if (!parseUInt32(value, version))
                {
                    errors.push_back("config.version must be an integer");
                    return false;
                }
                out_config.version = version;
                return true;
            }
            if (field == "debounce_ms")
            {
                uint32_t debounce_ms = 0;
                if (!parseUInt32(value, debounce_ms))
                {
                    errors.push_back("config.debounce_ms must be an integer");
                    return false;
                }
                out_config.debounce_ms = debounce_ms;
                return true;
            }
            if (field == "active_profile")
            {
                out_config.active_profile = value;
                return true;
            }

            errors.push_back("Unknown config field: " + field);
            return false;
        }

        if (state.section == Section::Profiles)
        {
            if (!state.has_current_profile)
            {
                state.current_profile = ProfileConfig{};
                state.has_current_profile = true;
            }

            if (field == "id")
            {
                state.current_profile.id = value;
            }
            else if (field == "label")
            {
                state.current_profile.label = value;
            }
            else
            {
                errors.push_back("Unknown profile field: " + field);
                return false;
            }

            return true;
        }

        if (state.section == Section::Keys || state.section == Section::ProfileKeys)
        {
            if (state.section == Section::ProfileKeys && !state.has_current_profile)
            {
                errors.push_back("profile keys require an active profile");
                return false;
            }
            if (!state.has_current_key)
            {
                state.current_key = KeyConfig{};
                state.has_current_key = true;
            }

            if (field == "id")
            {
                state.current_key.id = value;
            }
            else if (field == "label")
            {
                state.current_key.label = value;
            }
            else if (field == "icon")
            {
                state.current_key.icon = value;
            }
            else if (field == "action_id")
            {
                state.current_key.action_id = value;
            }
            else if (field == "key_index")
            {
                uint32_t index = 0;
                if (!parseUInt32(value, index))
                {
                    errors.push_back("key.key_index must be an integer");
                    return false;
                }
                state.current_key.key_index = static_cast<uint8_t>(index);
            }
            else if (field == "enabled")
            {
                bool enabled = true;
                if (!parseBool(value, enabled))
                {
                    errors.push_back("key.enabled must be true or false");
                    return false;
                }
                state.current_key.enabled = enabled;
            }
            else
            {
                errors.push_back("Unknown key field: " + field);
                return false;
            }

            return true;
        }

        if (state.section == Section::KeyActions)
        {
            if (!state.has_current_key_action)
            {
                state.current_key_action = ActionConfig{};
                state.has_current_key_action = true;
            }
            if (field == "id")
            {
                state.current_key_action.id = value;
            }
            else if (field == "type")
            {
                state.current_key_action.type = value;
            }
            else if (field == "payload")
            {
                state.current_key_action.payload = value;
            }
            else if (field == "method")
            {
                state.current_key_action.http_request.method = value;
                state.current_key_action.has_http_request = true;
            }
            else if (field == "url")
            {
                state.current_key_action.http_request.url = value;
                state.current_key_action.has_http_request = true;
            }
            else if (field == "body")
            {
                state.current_key_action.http_request.body = value;
                state.current_key_action.has_http_request = true;
            }
            else if (field == "timeout" || field == "timeout_ms")
            {
                uint32_t timeout_ms = 0;
                if (!parseUInt32(value, timeout_ms))
                {
                    errors.push_back("action.timeout_ms must be an integer");
                    return false;
                }
                state.current_key_action.http_request.timeout_ms = timeout_ms;
                state.current_key_action.has_http_request = true;
            }
            else if (field == "retries" || field == "retry")
            {
                uint32_t retries = 0;
                if (!parseUInt32(value, retries))
                {
                    errors.push_back("action.retries must be an integer");
                    return false;
                }
                state.current_key_action.http_request.retries = retries;
                state.current_key_action.has_http_request = true;
            }
            else if (field == "delay_ms")
            {
                uint32_t delay_ms = 0;
                if (!parseUInt32(value, delay_ms))
                {
                    errors.push_back("action.delay_ms must be an integer");
                    return false;
                }
                state.current_key_action.delay_ms = delay_ms;
            }
            else if (field == "repeat")
            {
                uint32_t repeat = 0;
                if (!parseUInt32(value, repeat))
                {
                    errors.push_back("action.repeat must be an integer");
                    return false;
                }
                state.current_key_action.repeat = repeat;
            }
            else if (field == "enabled")
            {
                bool enabled = true;
                if (!parseBool(value, enabled))
                {
                    errors.push_back("action.enabled must be true or false");
                    return false;
                }
                state.current_key_action.enabled = enabled;
            }
            else
            {
                errors.push_back("Unknown action field: " + field);
                return false;
            }

            return true;
        }

        if (state.section == Section::Actions || state.section == Section::ProfileActions)
        {
            if (state.section == Section::ProfileActions && !state.has_current_profile)
            {
                errors.push_back("profile actions require an active profile");
                return false;
            }
            if (!state.has_current_action)
            {
                state.current_action = ActionConfig{};
                state.has_current_action = true;
            }

            if (field == "id")
            {
                state.current_action.id = value;
            }
            else if (field == "type")
            {
                state.current_action.type = value;
            }
            else if (field == "payload")
            {
                state.current_action.payload = value;
            }
            else if (field == "method")
            {
                state.current_action.http_request.method = value;
                state.current_action.has_http_request = true;
            }
            else if (field == "url")
            {
                state.current_action.http_request.url = value;
                state.current_action.has_http_request = true;
            }
            else if (field == "body")
            {
                state.current_action.http_request.body = value;
                state.current_action.has_http_request = true;
            }
            else if (field == "timeout" || field == "timeout_ms")
            {
                uint32_t timeout_ms = 0;
                if (!parseUInt32(value, timeout_ms))
                {
                    errors.push_back("action.timeout_ms must be an integer");
                    return false;
                }
                state.current_action.http_request.timeout_ms = timeout_ms;
                state.current_action.has_http_request = true;
            }
            else if (field == "retries" || field == "retry")
            {
                uint32_t retries = 0;
                if (!parseUInt32(value, retries))
                {
                    errors.push_back("action.retries must be an integer");
                    return false;
                }
                state.current_action.http_request.retries = retries;
                state.current_action.has_http_request = true;
            }
            else if (field == "delay_ms")
            {
                uint32_t delay_ms = 0;
                if (!parseUInt32(value, delay_ms))
                {
                    errors.push_back("action.delay_ms must be an integer");
                    return false;
                }
                state.current_action.delay_ms = delay_ms;
            }
            else if (field == "repeat")
            {
                uint32_t repeat = 0;
                if (!parseUInt32(value, repeat))
                {
                    errors.push_back("action.repeat must be an integer");
                    return false;
                }
                state.current_action.repeat = repeat;
            }
            else if (field == "enabled")
            {
                bool enabled = true;
                if (!parseBool(value, enabled))
                {
                    errors.push_back("action.enabled must be true or false");
                    return false;
                }
                state.current_action.enabled = enabled;
            }
            else
            {
                errors.push_back("Unknown action field: " + field);
                return false;
            }

            return true;
        }

        errors.push_back("Unexpected YAML section");
        return false;
    };

    if (trimmed.rfind("-", 0) == 0)
    {
        std::string remainder = trim(trimmed.substr(1));
        if ((state.section == Section::ProfileKeys || state.section == Section::ProfileActions) && indent <= 2)
        {
            finalizeEntry(state, out_config);
            finalizeProfile(state, out_config);
            state.section = Section::Profiles;
        }

        if (state.section == Section::Profiles)
        {
            finalizeEntry(state, out_config);
            finalizeProfile(state, out_config);
            state.current_profile = ProfileConfig{};
            state.has_current_profile = true;
        }
        else if (state.section == Section::Keys || state.section == Section::ProfileKeys)
        {
            finalizeEntry(state, out_config);
            state.has_current_key = true;
        }
        else if (state.section == Section::Actions || state.section == Section::ProfileActions)
        {
            finalizeEntry(state, out_config);
            state.has_current_action = true;
        }
        else if (state.section == Section::KeyActions)
        {
            finalizeKeyAction(state);
            state.has_current_key_action = true;
        }
        else
        {
            errors.push_back("List item found outside keys/actions/profiles section");
            return false;
        }

        if (!remainder.empty())
        {
            auto pos = remainder.find(':');
            if (pos == std::string::npos)
            {
                errors.push_back("Malformed list entry: " + remainder);
                return false;
            }
            std::string field = trim(remainder.substr(0, pos));
            std::string value = trim(remainder.substr(pos + 1));
            value = stripQuotes(value);
            return assignField(field, value);
        }

        return true;
    }

    auto pos = trimmed.find(':');
    if (pos == std::string::npos)
    {
        errors.push_back("Malformed line: " + trimmed);
        return false;
    }

    std::string field = trim(trimmed.substr(0, pos));
    std::string value = trim(trimmed.substr(pos + 1));
    value = stripQuotes(value);
    return assignField(field, value);
}

void ConfigLoader::finalizeParse(ParseState &state, ConfigRoot &out_config)
{
    finalizeEntry(state, out_config);
    finalizeProfile(state, out_config);
}

void ConfigLoader::finalizeEntry(ParseState &state, ConfigRoot &out_config)
{
    finalizeKeyAction(state);
    if (state.has_current_key)
    {
        Section key_section = state.section;
        if (key_section == Section::KeyActions)
        {
            key_section = state.key_action_parent;
        }
        if (key_section == Section::ProfileKeys)
        {
            state.current_profile.keys.push_back(state.current_key);
        }
        else
        {
            out_config.keys.push_back(state.current_key);
        }
        state.has_current_key = false;
        state.current_key = KeyConfig{};
    }
    if (state.has_current_action)
    {
        Section action_section = state.section;
        if (action_section == Section::ActionSteps || action_section == Section::ActionHeaders)
        {
            action_section = state.action_detail_parent;
        }
        if (action_section == Section::ProfileActions)
        {
            state.current_profile.actions.push_back(state.current_action);
        }
        else
        {
            out_config.actions.push_back(state.current_action);
        }
        state.has_current_action = false;
        state.current_action = ActionConfig{};
    }
}

void ConfigLoader::finalizeProfile(ParseState &state, ConfigRoot &out_config)
{
    if (state.has_current_profile)
    {
        out_config.profiles.push_back(state.current_profile);
        state.has_current_profile = false;
        state.current_profile = ProfileConfig{};
    }
}

void ConfigLoader::finalizeKeyAction(ParseState &state)
{
    if (state.has_current_key_action)
    {
        state.current_key.actions.push_back(state.current_key_action);
        state.has_current_key_action = false;
        state.current_key_action = ActionConfig{};
    }
}

std::string ConfigLoader::trim(const std::string &value)
{
    const char *whitespace = " \t\n\r";
    const auto start = value.find_first_not_of(whitespace);
    if (start == std::string::npos)
    {
        return "";
    }
    const auto end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

std::string ConfigLoader::stripQuotes(const std::string &value)
{
    if (value.size() < 2)
    {
        return value;
    }

    char quote = value.front();
    if ((quote == '\"' || quote == '\'') && value.back() == quote)
    {
        return value.substr(1, value.size() - 2);
    }

    return value;
}

bool ConfigLoader::parseBool(const std::string &value, bool &out)
{
    if (value == "true")
    {
        out = true;
        return true;
    }
    if (value == "false")
    {
        out = false;
        return true;
    }
    return false;
}

bool ConfigLoader::parseUInt32(const std::string &value, uint32_t &out)
{
    if (value.empty())
    {
        return false;
    }

    char *end = nullptr;
    unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0')
    {
        return false;
    }

    out = static_cast<uint32_t>(parsed);
    return true;
}

void ConfigLoader::logErrors(const std::vector<std::string> &errors) const
{
    for (const auto &error : errors)
    {
        Serial.println(String("ConfigLoader: ") + error.c_str());
    }
}
