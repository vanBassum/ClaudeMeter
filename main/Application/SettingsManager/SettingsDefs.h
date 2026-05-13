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

    // Claude Code usage metering — populated by a host-side script that
    // probes Anthropic with the local OAuth token and POSTs the parsed
    // rate-limit headers (utilization % + reset times) to /api/usage.
    // Keys are capped at 15 chars by NVS — see SettingKey in SettingsManager.h.
    { "usage.auth_tok",  SettingType::String, "Ingest Auth Token (optional)", "" },
};

inline constexpr int SETTINGS_DEFS_COUNT = sizeof(SETTINGS_DEFS) / sizeof(SETTINGS_DEFS[0]);
