/*
 * @Description: Key event manager for T-Keyboard S3
 * @version: V1.0.0
 * @Author: None
 * @Date: 2024-04-09
 * @License: GPL 3.0
 */
#pragma once

#include <cstdint>

class KeyEventManager
{
public:
    static constexpr uint32_t kDefaultDebounceMs = 30;

    explicit KeyEventManager(uint32_t debounce_ms = kDefaultDebounceMs);

    void SetDebounceMs(uint32_t debounce_ms);
    uint32_t debounce_ms() const;

    uint8_t Update(bool key1, bool key2, bool key3, bool key4, uint32_t now_ms);
    uint8_t Update(uint32_t now_ms);

    void Reset();

private:
    static constexpr uint8_t kKeyCount = 4;

    uint8_t StableMask() const;
    uint8_t EventIdForMask(uint8_t mask) const;

    uint32_t debounce_ms_;
    bool last_raw_[kKeyCount]{};
    bool stable_[kKeyCount]{};
    uint32_t last_change_ms_[kKeyCount]{};
    uint8_t last_event_id_ = 0;
};
