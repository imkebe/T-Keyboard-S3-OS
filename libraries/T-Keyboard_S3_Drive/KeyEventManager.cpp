/*
 * @Description: Key event manager for T-Keyboard S3
 * @version: V1.0.0
 * @Author: None
 * @Date: 2024-04-09
 * @License: GPL 3.0
 */
#include "KeyEventManager.h"

#include <Arduino.h>

#include "T-Keyboard_S3_Drive.h"

KeyEventManager::KeyEventManager(uint32_t debounce_ms) : debounce_ms_(debounce_ms) {}

void KeyEventManager::SetDebounceMs(uint32_t debounce_ms)
{
    debounce_ms_ = debounce_ms;
}

uint32_t KeyEventManager::debounce_ms() const
{
    return debounce_ms_;
}

void KeyEventManager::Reset()
{
    for (uint8_t i = 0; i < kKeyCount; ++i)
    {
        last_raw_[i] = false;
        stable_[i] = false;
        last_change_ms_[i] = 0;
    }
    last_event_id_ = 0;
}

uint8_t KeyEventManager::Update(uint32_t now_ms)
{
    return Update(Key1_Flag, Key2_Flag, Key3_Flag, Key4_Flag, now_ms);
}

uint8_t KeyEventManager::Update(bool key1, bool key2, bool key3, bool key4, uint32_t now_ms)
{
    const bool raw[kKeyCount] = {key1, key2, key3, key4};
    for (uint8_t i = 0; i < kKeyCount; ++i)
    {
        if (raw[i] != last_raw_[i])
        {
            last_raw_[i] = raw[i];
            last_change_ms_[i] = now_ms;
        }

        if (raw[i] != stable_[i])
        {
            if (debounce_ms_ == 0 || (now_ms - last_change_ms_[i]) >= debounce_ms_)
            {
                stable_[i] = raw[i];
            }
        }
    }

    uint8_t event_id = EventIdForMask(StableMask());
    if (event_id != 0 && event_id != last_event_id_)
    {
        last_event_id_ = event_id;
        return event_id;
    }

    if (event_id == 0 && last_event_id_ != 0)
    {
        last_event_id_ = 0;
    }

    return 0;
}

uint8_t KeyEventManager::StableMask() const
{
    uint8_t mask = 0;
    for (uint8_t i = 0; i < kKeyCount; ++i)
    {
        if (stable_[i])
        {
            mask |= static_cast<uint8_t>(1U << i);
        }
    }
    return mask;
}

uint8_t KeyEventManager::EventIdForMask(uint8_t mask) const
{
    switch (mask)
    {
    case 0x0F:
        return 8;
    case 0x03:
        return 5;
    case 0x05:
        return 6;
    case 0x06:
        return 7;
    case 0x09:
        return 10;
    case 0x0C:
        return 9;
    case 0x01:
        return 1;
    case 0x02:
        return 2;
    case 0x04:
        return 3;
    case 0x08:
        return 4;
    default:
        return 0;
    }
}
