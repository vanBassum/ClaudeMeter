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

    // Claude Code usage metering — the device probes Anthropic itself using
    // a long-lived OAuth refresh token (copied from ~/.claude/.credentials.json
    // on a machine that has logged in to Claude Code). The access token is
    // refreshed automatically; only `claude.rt` survives across refreshes.
    // Keys are capped at 15 chars by NVS — see SettingKey in SettingsManager.h.
    { "claude.at",    SettingType::String, "Claude Access Token",   "" },
    { "claude.rt",    SettingType::String, "Claude Refresh Token",  "" },
    { "claude.exp",   SettingType::Int,    "Token Expires (unix)",  "0" },
    { "claude.poll",  SettingType::Int,    "Poll Interval (s)",     "60" },
};

inline constexpr int SETTINGS_DEFS_COUNT = sizeof(SETTINGS_DEFS) / sizeof(SETTINGS_DEFS[0]);
