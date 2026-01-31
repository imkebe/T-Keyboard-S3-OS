#pragma once

#include <string>
#include <vector>

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPIFFS.h>

#include "../../libraries/T-Keyboard_S3_Drive/ConfigSchema.h"

class ConfigLoader
{
public:
    ConfigLoader();

    bool reloadConfig();

    const ConfigRoot &config() const;
    const std::string &lastError() const;
    bool SetActiveProfile(const std::string &profile_id);

private:
    enum class Section
    {
        None,
        Config,
        Keys,
        Actions,
        Profiles,
        ProfileKeys,
        ProfileActions,
    };

    struct ParseState
    {
        Section section = Section::None;
        KeyConfig current_key;
        ActionConfig current_action;
        ProfileConfig current_profile;
        bool has_current_key = false;
        bool has_current_action = false;
        bool has_current_profile = false;
    };

    bool loadFromSd(ConfigRoot &out_config, std::vector<std::string> &errors);
    bool loadFromInternalFs(ConfigRoot &out_config, std::vector<std::string> &errors);
    bool loadFromFile(fs::FS &fs, const char *path, ConfigRoot &out_config, std::vector<std::string> &errors);

    bool parseLine(const std::string &line, ParseState &state, ConfigRoot &out_config, std::vector<std::string> &errors);
    void finalizeParse(ParseState &state, ConfigRoot &out_config);
    void finalizeEntry(ParseState &state, ConfigRoot &out_config);
    void finalizeProfile(ParseState &state, ConfigRoot &out_config);

    static std::string trim(const std::string &value);
    static std::string stripQuotes(const std::string &value);
    static bool parseBool(const std::string &value, bool &out);
    static bool parseUInt32(const std::string &value, uint32_t &out);

    void logErrors(const std::vector<std::string> &errors) const;

    ConfigRoot config_{};
    std::string last_error_{};
};
