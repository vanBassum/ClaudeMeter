#pragma once

#include "SettingsManager.h"

// ──────────────────────────────────────────────────────────────
// Setting definitions — add new settings here
// ──────────────────────────────────────────────────────────────

inline constexpr SettingDef SETTINGS_DEFS[] = {
    // WiFi
    { "wifi.ssid",      SettingType::String, "WiFi SSID",      "" },
    { "wifi.password",  SettingType::String, "WiFi Password",  "" },

    // Device
    { "device.name",    SettingType::String, "Device Name",    "Strux" },
    { "device.pin",     SettingType::String, "Device PIN",     "" },

    // MQTT
    { "mqtt.enabled",   SettingType::Bool,   "MQTT Enabled",   "0" },
    { "mqtt.broker",    SettingType::String, "MQTT Broker",    "" },
    { "mqtt.port",      SettingType::Int,    "MQTT Port",      "1883" },
    { "mqtt.user",      SettingType::String, "MQTT User",      "" },
    { "mqtt.pass",      SettingType::String, "MQTT Password",  "" },
    { "mqtt.prefix",    SettingType::String, "MQTT Prefix",    "strux" },

    // NTP
    { "ntp.server",     SettingType::String, "NTP Server",     "pool.ntp.org" },
    { "ntp.timezone",   SettingType::String, "NTP Timezone",   "UTC0" },

    // Claude API (rate-limit metering)
    { "claude.api_key",   SettingType::String, "Claude API Key",         "" },
    { "claude.model",     SettingType::String, "Claude Model",           "claude-haiku-4-5" },
    { "claude.poll_s",    SettingType::Int,    "Poll Interval (s, 0=off)", "60" },
};

inline constexpr int SETTINGS_DEFS_COUNT = sizeof(SETTINGS_DEFS) / sizeof(SETTINGS_DEFS[0]);
