#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include <USBHIDKeyboard.h>

class UsbHidMacroHandler
{
public:
    explicit UsbHidMacroHandler(USBHIDKeyboard &keyboard);

    void Execute(const std::string &payload);

private:
    enum class StepType
    {
        Press,
        Release,
        Text,
        Delay
    };

    struct Step
    {
        StepType type;
        std::string value;
        uint32_t delay_ms = 0;
    };

    USBHIDKeyboard &keyboard_;
    std::unordered_set<uint8_t> pressed_keys_;

    void RunStep(const Step &step);
    void ReleaseAll();
    static std::vector<Step> ParseSteps(const std::string &payload);
    static std::vector<std::string> SplitSteps(const std::string &payload);
    static std::vector<std::string> SplitKeys(const std::string &value);
    static std::string Trim(const std::string &value);
    static std::string StripQuotes(const std::string &value);
    static std::string ToUpper(const std::string &value);
    static bool ParseDelay(const std::string &value, uint32_t &out_delay);
    static bool ResolveKey(const std::string &value, uint8_t &out_key);
};
