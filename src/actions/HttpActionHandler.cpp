#include "HttpActionHandler.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

namespace
{
std::string Trim(const std::string &value)
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

std::string StripQuotes(const std::string &value)
{
    if (value.size() < 2)
    {
        return value;
    }
    char quote = value.front();
    if ((quote == '"' || quote == '\'') && value.back() == quote)
    {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::string ReplaceEscapedNewlines(const std::string &value)
{
    std::string result;
    result.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '\\' && i + 1 < value.size() && value[i + 1] == 'n')
        {
            result.push_back('\n');
            ++i;
        }
        else
        {
            result.push_back(value[i]);
        }
    }
    return result;
}

bool ParseUInt32(const std::string &value, uint32_t &out)
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

HttpRequestDefinition ParseHttpRequestDefinition(const std::string &payload)
{
    HttpRequestDefinition definition;
    std::string expanded = ReplaceEscapedNewlines(payload);
    bool in_headers = false;
    std::istringstream stream(expanded);
    std::string line;
    while (std::getline(stream, line))
    {
        std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed.front() == '#')
        {
            continue;
        }

        if (trimmed == "headers:")
        {
            in_headers = true;
            continue;
        }

        if (in_headers)
        {
            if (trimmed.rfind("-", 0) == 0)
            {
                std::string remainder = Trim(trimmed.substr(1));
                auto pos = remainder.find(':');
                if (pos != std::string::npos)
                {
                    std::string key = Trim(remainder.substr(0, pos));
                    std::string value = Trim(remainder.substr(pos + 1));
                    definition.headers[key] = StripQuotes(value);
                }
                continue;
            }
            in_headers = false;
        }

        auto pos = trimmed.find(':');
        if (pos == std::string::npos)
        {
            continue;
        }
        std::string field = Trim(trimmed.substr(0, pos));
        std::string value = StripQuotes(Trim(trimmed.substr(pos + 1)));

        if (field == "method")
        {
            definition.method = value;
        }
        else if (field == "url")
        {
            definition.url = value;
        }
        else if (field == "body")
        {
            definition.body = value;
        }
        else if (field == "timeout_ms" || field == "timeout")
        {
            uint32_t timeout = 0;
            if (ParseUInt32(value, timeout))
            {
                definition.timeout_ms = timeout;
            }
        }
        else if (field == "retries" || field == "retry")
        {
            uint32_t retries = 0;
            if (!ParseUInt32(value, retries))
            {
                Serial.println(String("HttpActionHandler: invalid retries value ") + value.c_str());
                continue;
            }
            if (retries > ConfigLimits::kMaxHttpRetries)
            {
                Serial.println(String("HttpActionHandler: clamping retries from ") + String(retries) + " to " +
                               String(ConfigLimits::kMaxHttpRetries));
                retries = ConfigLimits::kMaxHttpRetries;
            }
            definition.retries = retries;
        }
    }
    return definition;
}

bool IsSuccessStatus(int status_code)
{
    return status_code >= 200 && status_code < 300;
}
} // namespace

ActionStatus HandleHttpRequestAction(const ActionConfig &action)
{
    HttpRequestDefinition definition = ParseHttpRequestDefinition(action.payload);
    if (definition.url.empty())
    {
        return ActionStatus::Failure(-1, "http_request missing url");
    }

    std::string method = definition.method;
    std::transform(method.begin(), method.end(), method.begin(), [](unsigned char value) {
        return static_cast<char>(std::toupper(value));
    });

    uint32_t attempts = definition.retries + 1;
    WiFiClient client;

    for (uint32_t attempt = 0; attempt < attempts; ++attempt)
    {
        HTTPClient http;
        if (!http.begin(client, definition.url.c_str()))
        {
            continue;
        }

        http.setTimeout(definition.timeout_ms);
        for (const auto &entry : definition.headers)
        {
            http.addHeader(entry.first.c_str(), entry.second.c_str());
        }

        int status_code = 0;
        if (!definition.body.empty())
        {
            status_code = http.sendRequest(method.c_str(), definition.body.c_str());
        }
        else
        {
            status_code = http.sendRequest(method.c_str());
        }

        http.end();

        if (status_code > 0 && IsSuccessStatus(status_code))
        {
            return ActionStatus{true, status_code, "ok"};
        }

        if (status_code > 0)
        {
            return ActionStatus::Failure(status_code, "http_request failed");
        }
    }

    return ActionStatus::Failure(-1, "http_request failed to connect");
}
