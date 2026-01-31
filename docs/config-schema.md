# T-Keyboard S3 Configuration Schema (v1)

This document defines the versioned YAML configuration schema used to describe key layouts and actions.

## Root Object

```yaml
config:
  version: 1
  debounce_ms: 30
  active_profile: "default"
profiles:
  - id: "default"
    label: "Primary"
    keys:
      - id: "key-1"
        key_index: 0
        label: "Layer"
        icon: "sd:/icons/layer.png"
        action_id: "layer-1"
        enabled: true
    actions:
      - id: "layer-1"
        type: "layer"
        payload: "fn"
        enabled: true
```

### `config`
| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `version` | integer | ✅ | Schema version. Only `1` is currently supported. |
| `debounce_ms` | integer | ❌ | Key debounce duration in milliseconds. Defaults to `30`. |
| `active_profile` | string | ❌ | Active `profiles[].id`. Defaults to the first profile when omitted. |

### `profiles[]`
| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `id` | string | ✅ | Unique identifier for the profile. |
| `label` | string | ❌ | Friendly name for the profile. |
| `keys` | array | ❌ | Collection of key mappings for the profile. |
| `actions` | array | ❌ | Collection of actions for the profile. |

### `keys[]`
| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `id` | string | ✅ | Unique identifier for the key mapping. |
| `key_index` | integer | ❌ | Zero-based hardware key index (0-255). Defaults to `0`. |
| `label` | string | ❌ | Human-friendly display name. |
| `icon` | string | ❌ | Path to an icon asset on SD (`sd:/...`) or internal FS (`fs:/...`). |
| `action_id` | string | ❌ | Reference to an `actions[].id`. If omitted, the key has no bound action. |
| `enabled` | boolean | ❌ | Enables or disables the key mapping. Defaults to `true`. |

### `actions[]`
| Field | Type | Required | Description |
| --- | --- | --- | --- |
| `id` | string | ✅ | Unique identifier for the action. |
| `type` | string | ✅ | Action type. Allowed values: `hid_key`, `ble_key`, `http_request`, `composite`, `macro`, `media`, `keycode`, `layer`, `system`, `custom`, `profile_switch`. |
| `payload` | string | ❌ | Action payload, such as a macro string or keycode. |
| `enabled` | boolean | ❌ | Enables or disables the action. Defaults to `true`. |

## Validation Rules

- `config.version` must be `1`.
- `profiles[].id` values must be unique and non-empty (when profiles are provided).
- `keys[].id` values must be unique and non-empty within their profile.
- `actions[].id` values must be unique and non-empty within their profile.
- `actions[].type` must be one of the allowed values.
- If `keys[].action_id` is set, it must reference an existing `actions[].id` in the same profile.
- If `profiles[]` is omitted, `keys[]`/`actions[]` are read from the root object.

## Data Model Alignment

The schema maps directly to the following C++ structs:

- `ConfigRoot` → root object containing `version`, `keys`, `actions`, `profiles`, and `active_profile`.
- `ProfileConfig` → entries in `profiles[]`.
- `KeyConfig` → entries in `keys[]`.
- `ActionConfig` → entries in `actions[]`.

Validation behavior is implemented in the `Validate()` methods on each struct, with additional cross-field checks in `ConfigRoot::Validate()`.
