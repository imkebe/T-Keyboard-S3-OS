#include "UsbHidMacroHandler.h"

#include <Arduino.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <unordered_map>

#include "../../libraries/T-Keyboard_S3_Drive/ConfigSchema.h"

namespace
{
const std::unordered_map<std::string, uint8_t> &KeyLookup()
{
    static const std::unordered_map<std::string, uint8_t> lookup{
        {"CTRL", KEY_LEFT_CTRL},
        {"CONTROL", KEY_LEFT_CTRL},
        {"LCTRL", KEY_LEFT_CTRL},
        {"RCTRL", KEY_RIGHT_CTRL},
        {"SHIFT", KEY_LEFT_SHIFT},
        {"LSHIFT", KEY_LEFT_SHIFT},
        {"RSHIFT", KEY_RIGHT_SHIFT},
        {"ALT", KEY_LEFT_ALT},
        {"LALT", KEY_LEFT_ALT},
        {"RALT", KEY_RIGHT_ALT},
        {"GUI", KEY_LEFT_GUI},
        {"WIN", KEY_LEFT_GUI},
        {"WINDOWS", KEY_LEFT_GUI},
        {"CMD", KEY_LEFT_GUI},
        {"COMMAND", KEY_LEFT_GUI},
        {"META", KEY_LEFT_GUI},
        {"ENTER", KEY_RETURN},
        {"RETURN", KEY_RETURN},
        {"ESC", KEY_ESC},
        {"ESCAPE", KEY_ESC},
        {"TAB", KEY_TAB},
        {"BACKSPACE", KEY_BACKSPACE},
        {"SPACE", ' '},
        {"SPACEBAR", ' '},
        {"UP", KEY_UP_ARROW},
        {"DOWN", KEY_DOWN_ARROW},
        {"LEFT", KEY_LEFT_ARROW},
        {"RIGHT", KEY_RIGHT_ARROW},
        {"INSERT", KEY_INSERT},
        {"INS", KEY_INSERT},
        {"DELETE", KEY_DELETE},
        {"DEL", KEY_DELETE},
        {"HOME", KEY_HOME},
        {"END", KEY_END},
        {"PAGEUP", KEY_PAGE_UP},
        {"PGUP", KEY_PAGE_UP},
        {"PAGEDOWN", KEY_PAGE_DOWN},
        {"PGDN", KEY_PAGE_DOWN},
        {"CAPSLOCK", KEY_CAPS_LOCK},
        {"PRINTSCREEN", KEY_PRINT_SCREEN},
        {"PRTSC", KEY_PRINT_SCREEN},
        {"SCROLLLOCK", KEY_SCROLL_LOCK},
        {"PAUSE", KEY_PAUSE},
        {"NUMLOCK", KEY_NUM_LOCK},
        {"F1", KEY_F1},
        {"F2", KEY_F2},
        {"F3", KEY_F3},
        {"F4", KEY_F4},
        {"F5", KEY_F5},
        {"F6", KEY_F6},
        {"F7", KEY_F7},
        {"F8", KEY_F8},
        {"F9", KEY_F9},
        {"F10", KEY_F10},
        {"F11", KEY_F11},
        {"F12", KEY_F12},
    };
    return lookup;
}

bool ResolveFunctionKey(const std::string &value, uint8_t &out_key)
{
    if (value.size() < 2 || value[0] != 'F')
    {
        return false;
    }

    int number = 0;
    for (size_t i = 1; i < value.size(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(value[i])))
        {
            return false;
        }
        number = number * 10 + (value[i] - '0');
    }

    switch (number)
    {
    case 1:
        out_key = KEY_F1;
        return true;
    case 2:
        out_key = KEY_F2;
        return true;
    case 3:
        out_key = KEY_F3;
        return true;
    case 4:
        out_key = KEY_F4;
        return true;
    case 5:
        out_key = KEY_F5;
        return true;
    case 6:
        out_key = KEY_F6;
        return true;
    case 7:
        out_key = KEY_F7;
        return true;
    case 8:
        out_key = KEY_F8;
        return true;
    case 9:
        out_key = KEY_F9;
        return true;
    case 10:
        out_key = KEY_F10;
        return true;
    case 11:
        out_key = KEY_F11;
        return true;
    case 12:
        out_key = KEY_F12;
        return true;
    default:
        return false;
    }
}
} // namespace

UsbHidMacroHandler::UsbHidMacroHandler(USBHIDKeyboard &keyboard) : keyboard_(keyboard) {}

void UsbHidMacroHandler::Execute(const std::string &payload)
{
    auto steps = ParseSteps(payload);
    for (const auto &step : steps)
    {
        RunStep(step);
    }
    ReleaseAll();
}

void UsbHidMacroHandler::RunStep(const Step &step)
{
    switch (step.type)
    {
    case StepType::Press: {
        auto keys = SplitKeys(step.value);
        for (const auto &key : keys)
        {
            uint8_t keycode = 0;
            if (!ResolveKey(key, keycode))
            {
                Serial.println(String("UsbHidMacroHandler: unknown key ") + key.c_str());
                continue;
            }
            keyboard_.press(keycode);
            pressed_keys_.insert(keycode);
        }
        break;
    }
    case StepType::Release: {
        auto keys = SplitKeys(step.value);
        for (const auto &key : keys)
        {
            uint8_t keycode = 0;
            if (!ResolveKey(key, keycode))
            {
                Serial.println(String("UsbHidMacroHandler: unknown key ") + key.c_str());
                continue;
            }
            keyboard_.release(keycode);
            pressed_keys_.erase(keycode);
        }
        break;
    }
    case StepType::Text:
        keyboard_.print(step.value.c_str());
        break;
    case StepType::Delay:
        if (step.delay_ms > 0)
        {
            delay(step.delay_ms);
        }
        break;
    }
}

void UsbHidMacroHandler::ReleaseAll()
{
    for (uint8_t keycode : pressed_keys_)
    {
        keyboard_.release(keycode);
    }
    pressed_keys_.clear();
}

std::vector<UsbHidMacroHandler::Step> UsbHidMacroHandler::ParseSteps(const std::string &payload)
{
    std::vector<Step> steps;
    for (const auto &raw : SplitSteps(payload))
    {
        std::string entry = Trim(raw);
        if (entry.empty())
        {
            continue;
        }
        if (entry.rfind('-', 0) == 0)
        {
            entry = Trim(entry.substr(1));
        }
        auto pos = entry.find(':');
        if (pos == std::string::npos)
        {
            Serial.println(String("UsbHidMacroHandler: malformed step ") + entry.c_str());
            continue;
        }
        std::string key = ToUpper(Trim(entry.substr(0, pos)));
        std::string value = StripQuotes(Trim(entry.substr(pos + 1)));

        if (key == "PRESS")
        {
            steps.push_back({StepType::Press, value, 0});
        }
        else if (key == "RELEASE")
        {
            steps.push_back({StepType::Release, value, 0});
        }
        else if (key == "TEXT")
        {
            steps.push_back({StepType::Text, value, 0});
        }
        else if (key == "DELAY_MS" || key == "DELAY")
        {
            uint32_t delay_ms = 0;
            if (!ParseDelay(value, delay_ms))
            {
                Serial.println(String("UsbHidMacroHandler: invalid delay ") + value.c_str() +
                               " (max " + String(ConfigLimits::kMaxMacroDelayMs) + ")");
                continue;
            }
            steps.push_back({StepType::Delay, {}, delay_ms});
        }
        else
        {
            Serial.println(String("UsbHidMacroHandler: unknown step ") + key.c_str());
        }
    }
    return steps;
}

std::vector<std::string> UsbHidMacroHandler::SplitSteps(const std::string &payload)
{
    std::vector<std::string> parts;
    std::string current;
    for (char ch : payload)
    {
        if (ch == ';' || ch == '\n' || ch == '|')
        {
            parts.push_back(current);
            current.clear();
        }
        else
        {
            current.push_back(ch);
        }
    }
    if (!current.empty())
    {
        parts.push_back(current);
    }
    return parts;
}

std::vector<std::string> UsbHidMacroHandler::SplitKeys(const std::string &value)
{
    std::vector<std::string> keys;
    std::string current;
    for (char ch : value)
    {
        if (ch == '+' || ch == ',')
        {
            if (!current.empty())
            {
                keys.push_back(Trim(current));
                current.clear();
            }
        }
        else
        {
            current.push_back(ch);
        }
    }
    if (!current.empty())
    {
        keys.push_back(Trim(current));
    }
    return keys;
}

std::string UsbHidMacroHandler::Trim(const std::string &value)
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

std::string UsbHidMacroHandler::StripQuotes(const std::string &value)
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

std::string UsbHidMacroHandler::ToUpper(const std::string &value)
{
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return out;
}

bool UsbHidMacroHandler::ParseDelay(const std::string &value, uint32_t &out_delay)
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
    if (parsed > ConfigLimits::kMaxMacroDelayMs)
    {
        return false;
    }
    out_delay = static_cast<uint32_t>(parsed);
    return true;
}

bool UsbHidMacroHandler::ResolveKey(const std::string &value, uint8_t &out_key)
{
    std::string trimmed = Trim(value);
    if (trimmed.empty())
    {
        return false;
    }
    if (trimmed.size() == 1)
    {
        out_key = static_cast<uint8_t>(trimmed[0]);
        return true;
    }

    std::string normalized = ToUpper(trimmed);
    auto lookup = KeyLookup();
    auto it = lookup.find(normalized);
    if (it != lookup.end())
    {
        out_key = it->second;
        return true;
    }

    return ResolveFunctionKey(normalized, out_key);
}
