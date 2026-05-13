# Brainstorm: Settings System Refactor

**Status:** Idea — not ready to implement yet.

## The Idea

Move setting definitions out of the central `SettingsDefs.h` into the classes that own and use them. Each manager declares `SettingDefinition` member variables and registers them at runtime with SettingsManager.

```cpp
class NetworkManager {
    SettingDefinition wifiSsid_{"wifi.ssid", SettingType::String, "WiFi SSID", ""};
    SettingDefinition wifiPass_{"wifi.password", SettingType::String, "WiFi Password", ""};
public:
    void Init() {
        settingsManager.Register(wifiSsid_);
        settingsManager.Register(wifiPass_);
    }
    void Connect() {
        char ssid[32];
        wifiSsid_.get(ssid);
    }
};
```

## Why It's Worth Doing

Eliminates shotgun surgery: today adding a setting means editing `SettingsDefs.h` + `SettingsManager.cpp` + the consumer. With this design, a new setting lives entirely in the class that needs it.

## Tradeoffs

**Cons of distributed definitions:**
- Bloats manager classes — settings metadata mixed with business logic
- No single place to see all settings at a glance
- Runtime registration: a fixed-size registry can be full (silent failure risk)

**Pros of distributed definitions:**
- Manager becomes self-contained — doesn't change when unrelated settings are added elsewhere
- Co-location: the class that owns the setting also owns its defaults and label

**On namespace keys:** `Settings::Wifi::Ssid` feels like drag — hard-typed but verbose. Plain strings are the natural alternative; less ceremony, slightly more error-prone, probably acceptable given tests.

**On componentization:** Componentization and "adjustable" pull in opposite directions. This codebase leans adjustable — everything in one main component, easy to change. Only generic infrastructure (`lib/`) is stable enough to be a reusable component.

---

## Option: Runtime-Injected Definitions

**Core idea:** SettingsManager stops knowing about the app schema. The definition table is passed in from outside — making it a generic, reusable NVS-backed schema store.

```cpp
// Application owns the schema
inline const SettingDef APP_SETTINGS[] = {
    { "wifi.ssid",     SettingType::String, "WiFi SSID",     "" },
    { "wifi.password", SettingType::String, "WiFi Password", "" },
    // ...
};

// Injected at construction or Init
SettingsManager sm{serviceProvider, APP_SETTINGS, std::size(APP_SETTINGS)};
```

Consumers access settings the same way as today — `getString("wifi.ssid", buf, sizeof(buf))`.

**What changes:**
- `SettingsManager` constructor or `Init()` takes `const SettingDef* defs, int count`
- `SettingsDefs.h` moves to the application layer — out of the SettingsManager folder
- SettingsManager has zero app-specific includes → can move to `lib/`

**What stays the same:**
- Central table (one overview of all settings)
- `get/setString/Int/Bool` API unchanged
- `WriteAllSettings()` unchanged
- No runtime registration, no fixed-size registry cap

**Tradeoffs:**
- Pro: SettingsManager becomes a reusable lib component, decoupled from the app
- Pro: Minimal change from current design — surgical
- Pro: Still one place to see all settings
- Con: Still shotgun surgery when adding a setting (table + consumer)
- Con: String keys unvalidated at compile time unless constants are kept
