#pragma once

#include <string>

struct ActionStatus
{
    bool success = true;
    int status_code = 0;
    std::string message;

    static ActionStatus Ok()
    {
        return ActionStatus{};
    }

    static ActionStatus Failure(int code, const std::string &reason)
    {
        ActionStatus status;
        status.success = false;
        status.status_code = code;
        status.message = reason;
        return status;
    }
};
