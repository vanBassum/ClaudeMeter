#pragma once

#include "ServiceProvider.h"
#include "InitState.h"
#include "Task.h"
#include "Mutex.h"
#include "DateTime.h"
#include "esp_http_client.h"
#include <cstdint>

// Probes Anthropic directly using a long-lived OAuth refresh token persisted
// in NVS (originally copied from ~/.claude/.credentials.json on a machine
// that has logged in to Claude Code). The access token is refreshed on
// expiry; tokens are persisted back to NVS so the device survives reboots.
//
// The data the device displays is parsed from the `anthropic-ratelimit-
// unified-*` response headers on a minimal /v1/messages probe — the same
// trick Claude Code itself uses for its /usage panel.
class ClaudeMeterManager
{
    static constexpr const char *TAG = "ClaudeMeterManager";

    // OAuth client + scopes match Claude Code's CLI (cli.js, GHA block).
    static constexpr const char *OAUTH_CLIENT_ID  = "9d1c250a-e61b-44d9-88ed-5944d1962f5e";
    static constexpr const char *OAUTH_TOKEN_URL  = "https://platform.claude.com/v1/oauth/token";
    static constexpr const char *OAUTH_SCOPES     = "user:profile user:inference user:sessions:claude_code user:mcp_servers";
    static constexpr const char *ANTHROPIC_BETA   = "oauth-2025-04-20";
    static constexpr const char *ANTHROPIC_VER    = "2023-06-01";
    static constexpr const char *MESSAGES_URL     = "https://api.anthropic.com/v1/messages";
    static constexpr const char *PROBE_MODEL      = "claude-haiku-4-5";

    // Refresh proactively N seconds before the stored expiry.
    static constexpr int REFRESH_LEAD_SECS = 120;

public:
    enum class Status {
        Unconfigured,      // no refresh token in settings
        WaitingForNetwork,
        Refreshing,
        Probing,
        Ok,
        AuthError,         // refresh failed (token revoked, etc.)
        RateLimited,
        NetworkError,
        OtherError
    };

    struct Snapshot {
        Status   status              = Status::Unconfigured;
        bool     everSucceeded       = false;
        DateTime updatedAt{};
        int      httpStatus          = 0;

        // -1 = header not seen this poll.
        int     fiveHourUtilPct      = -1;
        int64_t fiveHourResetUnix    = 0;
        int     sevenDayUtilPct      = -1;
        int64_t sevenDayResetUnix    = 0;
    };

    explicit ClaudeMeterManager(ServiceProvider &sp);
    ClaudeMeterManager(const ClaudeMeterManager &) = delete;
    ClaudeMeterManager &operator=(const ClaudeMeterManager &) = delete;

    void Init();

    Snapshot GetSnapshot();
    static const char *StatusStr(Status s);

private:
    ServiceProvider &serviceProvider_;
    InitState        initState_;
    Task             task_;
    Mutex            mutex_;
    Snapshot         snapshot_;

    // Hot-state mirror of settings (refreshed each loop iteration).
    int     pollSec_           = 60;
    char    accessToken_[256]  = {};
    char    refreshToken_[256] = {};
    int64_t expiresAtUnix_     = 0;

    // Captured by the HTTP event handler during a probe.
    int     hdrFiveHourUtilPct   = -1;
    int64_t hdrFiveHourResetUnix = 0;
    int     hdrSevenDayUtilPct   = -1;
    int64_t hdrSevenDayResetUnix = 0;

    // Buffer for refresh response body, populated by event handler.
    char    refreshBody_[1024] = {};
    size_t  refreshBodyLen_    = 0;

    void Work();
    void LoadConfig();
    void SetStatus(Status s);

    bool TokenNeedsRefresh() const;
    bool DoRefresh();
    bool DoProbe();

    void PersistTokens();
    void ResetHeaderCapture();
    void HandleHeader(const char *key, const char *value);

    static esp_err_t ProbeEventHandler(esp_http_client_event_t *evt);
    static esp_err_t RefreshEventHandler(esp_http_client_event_t *evt);
};
