#include "ClaudeMeterManager.h"
#include "NetworkManager/NetworkManager.h"
#include "SettingsManager/SettingsManager.h"
#include "ContextLock.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include <cstring>
#include <strings.h>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <ctime>

ClaudeMeterManager::ClaudeMeterManager(ServiceProvider &sp)
    : serviceProvider_(sp)
{
}

void ClaudeMeterManager::Init()
{
    auto init = initState_.TryBeginInit();
    if (!init) return;

    ESP_LOGI(TAG, "Initializing ClaudeMeterManager...");
    LoadConfig();
    task_.Init("ClaudeMeter", 4, 8192);
    task_.SetHandler([this]() { Work(); });
    task_.Run();

    init.SetReady();
    ESP_LOGI(TAG, "ClaudeMeterManager initialized.");
}

ClaudeMeterManager::Snapshot ClaudeMeterManager::GetSnapshot()
{
    LOCK(mutex_);
    return snapshot_;
}

const char *ClaudeMeterManager::StatusStr(Status s)
{
    switch (s) {
        case Status::Unconfigured:      return "Set claude.rt in settings";
        case Status::WaitingForNetwork: return "Waiting for network";
        case Status::Refreshing:        return "Refreshing token...";
        case Status::Probing:           return "Probing...";
        case Status::Ok:                return "OK";
        case Status::AuthError:         return "Auth error — re-paste tokens";
        case Status::RateLimited:       return "Rate-limited";
        case Status::NetworkError:      return "Network error";
        case Status::OtherError:        return "API error";
    }
    return "?";
}

void ClaudeMeterManager::LoadConfig()
{
    auto &settings = serviceProvider_.getSettingsManager();
    accessToken_[0]  = '\0';
    refreshToken_[0] = '\0';
    settings.getString("claude.at",  accessToken_,  sizeof(accessToken_));
    settings.getString("claude.rt",  refreshToken_, sizeof(refreshToken_));
    expiresAtUnix_   = (int64_t)settings.getInt("claude.exp",  0);
    pollSec_         =          settings.getInt("claude.poll", 60);
    if (pollSec_ < 10) pollSec_ = 10;
}

void ClaudeMeterManager::SetStatus(Status s)
{
    LOCK(mutex_);
    snapshot_.status = s;
}

void ClaudeMeterManager::Work()
{
    auto &network = serviceProvider_.getNetworkManager();

    while (true)
    {
        LoadConfig();

        if (refreshToken_[0] == '\0')
        {
            SetStatus(Status::Unconfigured);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        if (!network.wifi().getStatus().has_ipv4)
        {
            SetStatus(Status::WaitingForNetwork);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        if (accessToken_[0] == '\0' || TokenNeedsRefresh())
        {
            SetStatus(Status::Refreshing);
            if (!DoRefresh())
            {
                // DoRefresh sets the status (AuthError / NetworkError) itself.
                vTaskDelay(pdMS_TO_TICKS(60 * 1000));
                continue;
            }
        }

        SetStatus(Status::Probing);
        DoProbe();

        vTaskDelay(pdMS_TO_TICKS(pollSec_ * 1000));
    }
}

bool ClaudeMeterManager::TokenNeedsRefresh() const
{
    if (expiresAtUnix_ == 0) return true;
    int64_t now = (int64_t)time(nullptr);
    return (now + REFRESH_LEAD_SECS) >= expiresAtUnix_;
}

// ── JSON extraction helpers ──────────────────────────────────────

static const char *find_key(const char *json, const char *key)
{
    char needle[48];
    int n = snprintf(needle, sizeof(needle), "\"%s\"", key);
    if (n < 0 || n >= (int)sizeof(needle)) return nullptr;
    const char *p = strstr(json, needle);
    if (!p) return nullptr;
    p += n;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != ':') return nullptr;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static bool json_string(const char *json, const char *key, char *out, size_t outCap)
{
    const char *p = find_key(json, key);
    if (!p || *p != '"') return false;
    p++;
    const char *start = p;
    while (*p && *p != '"')
    {
        if (*p == '\\' && p[1]) p++;
        p++;
    }
    if (*p != '"') return false;
    size_t len = (size_t)(p - start);
    if (len >= outCap) return false;
    memcpy(out, start, len);
    out[len] = '\0';
    return true;
}

static int64_t json_int(const char *json, const char *key, int64_t fallback)
{
    const char *p = find_key(json, key);
    if (!p) return fallback;
    char *end = nullptr;
    long long v = strtoll(p, &end, 10);
    if (end == p) return fallback;
    return (int64_t)v;
}

// ── OAuth refresh ────────────────────────────────────────────────

esp_err_t ClaudeMeterManager::RefreshEventHandler(esp_http_client_event_t *evt)
{
    auto *self = static_cast<ClaudeMeterManager *>(evt->user_data);
    if (!self) return ESP_OK;
    if (evt->event_id == HTTP_EVENT_ON_DATA && evt->data && evt->data_len > 0)
    {
        size_t room = sizeof(self->refreshBody_) - 1 - self->refreshBodyLen_;
        size_t n = (size_t)evt->data_len < room ? (size_t)evt->data_len : room;
        if (n > 0)
        {
            memcpy(self->refreshBody_ + self->refreshBodyLen_, evt->data, n);
            self->refreshBodyLen_ += n;
            self->refreshBody_[self->refreshBodyLen_] = '\0';
        }
    }
    return ESP_OK;
}

bool ClaudeMeterManager::DoRefresh()
{
    refreshBody_[0] = '\0';
    refreshBodyLen_ = 0;

    char body[768];
    int n = snprintf(body, sizeof(body),
        "{\"grant_type\":\"refresh_token\","
         "\"refresh_token\":\"%s\","
         "\"client_id\":\"%s\","
         "\"scope\":\"%s\"}",
        refreshToken_, OAUTH_CLIENT_ID, OAUTH_SCOPES);
    if (n < 0 || n >= (int)sizeof(body))
    {
        ESP_LOGE(TAG, "Refresh body too large");
        SetStatus(Status::OtherError);
        return false;
    }

    esp_http_client_config_t cfg = {};
    cfg.url               = OAUTH_TOKEN_URL;
    cfg.method            = HTTP_METHOD_POST;
    cfg.event_handler     = RefreshEventHandler;
    cfg.user_data         = this;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.timeout_ms        = 20000;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "content-type", "application/json");
    esp_http_client_set_post_field(client, body, n);

    esp_err_t err = esp_http_client_perform(client);
    int httpSts   = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Refresh transport error: %s", esp_err_to_name(err));
        SetStatus(Status::NetworkError);
        return false;
    }
    if (httpSts < 200 || httpSts >= 300)
    {
        ESP_LOGW(TAG, "Refresh HTTP %d body=%.200s", httpSts, refreshBody_);
        SetStatus(Status::AuthError);
        return false;
    }

    char newAccess[256] = {};
    if (!json_string(refreshBody_, "access_token", newAccess, sizeof(newAccess)))
    {
        ESP_LOGE(TAG, "Refresh: no access_token in response");
        SetStatus(Status::AuthError);
        return false;
    }
    int64_t expiresIn = json_int(refreshBody_, "expires_in", 3600);

    char newRefresh[256] = {};
    bool gotNewRefresh = json_string(refreshBody_, "refresh_token", newRefresh, sizeof(newRefresh));

    strncpy(accessToken_, newAccess, sizeof(accessToken_) - 1);
    accessToken_[sizeof(accessToken_) - 1] = '\0';
    if (gotNewRefresh && newRefresh[0] != '\0')
    {
        strncpy(refreshToken_, newRefresh, sizeof(refreshToken_) - 1);
        refreshToken_[sizeof(refreshToken_) - 1] = '\0';
    }
    expiresAtUnix_ = (int64_t)time(nullptr) + expiresIn;

    PersistTokens();
    ESP_LOGI(TAG, "Refreshed; expires in %lld s", (long long)expiresIn);
    return true;
}

void ClaudeMeterManager::PersistTokens()
{
    auto &settings = serviceProvider_.getSettingsManager();
    settings.setString("claude.at",  accessToken_);
    settings.setString("claude.rt",  refreshToken_);
    settings.setInt   ("claude.exp", (int32_t)expiresAtUnix_);
    settings.Save();
}

// ── Probe ────────────────────────────────────────────────────────

void ClaudeMeterManager::ResetHeaderCapture()
{
    hdrFiveHourUtilPct   = -1;
    hdrFiveHourResetUnix = 0;
    hdrSevenDayUtilPct   = -1;
    hdrSevenDayResetUnix = 0;
}

void ClaudeMeterManager::HandleHeader(const char *key, const char *value)
{
    if (strcasecmp(key, "anthropic-ratelimit-unified-5h-utilization") == 0)
        hdrFiveHourUtilPct = (int)(atof(value) * 100.0 + 0.5);
    else if (strcasecmp(key, "anthropic-ratelimit-unified-5h-reset") == 0)
        hdrFiveHourResetUnix = atoll(value);
    else if (strcasecmp(key, "anthropic-ratelimit-unified-7d-utilization") == 0)
        hdrSevenDayUtilPct = (int)(atof(value) * 100.0 + 0.5);
    else if (strcasecmp(key, "anthropic-ratelimit-unified-7d-reset") == 0)
        hdrSevenDayResetUnix = atoll(value);
}

esp_err_t ClaudeMeterManager::ProbeEventHandler(esp_http_client_event_t *evt)
{
    auto *self = static_cast<ClaudeMeterManager *>(evt->user_data);
    if (!self) return ESP_OK;
    if (evt->event_id == HTTP_EVENT_ON_HEADER && evt->header_key && evt->header_value)
        self->HandleHeader(evt->header_key, evt->header_value);
    return ESP_OK;
}

bool ClaudeMeterManager::DoProbe()
{
    ResetHeaderCapture();

    esp_http_client_config_t cfg = {};
    cfg.url               = MESSAGES_URL;
    cfg.method            = HTTP_METHOD_POST;
    cfg.event_handler     = ProbeEventHandler;
    cfg.user_data         = this;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.timeout_ms        = 15000;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);

    char auth[280];
    snprintf(auth, sizeof(auth), "Bearer %s", accessToken_);
    esp_http_client_set_header(client, "Authorization", auth);
    esp_http_client_set_header(client, "anthropic-version", ANTHROPIC_VER);
    esp_http_client_set_header(client, "anthropic-beta", ANTHROPIC_BETA);
    esp_http_client_set_header(client, "content-type", "application/json");

    char body[192];
    int n = snprintf(body, sizeof(body),
        "{\"model\":\"%s\",\"max_tokens\":1,"
         "\"messages\":[{\"role\":\"user\",\"content\":\"quota\"}]}",
        PROBE_MODEL);
    esp_http_client_set_post_field(client, body, n);

    esp_err_t err = esp_http_client_perform(client);
    int httpSts   = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    Snapshot result;
    {
        LOCK(mutex_);
        result = snapshot_;
    }
    result.httpStatus = httpSts;
    // Only overwrite a row when we actually captured a value this poll;
    // otherwise keep the last-known utilisation so the bars don't flicker.
    if (hdrFiveHourUtilPct >= 0)
    {
        result.fiveHourUtilPct   = hdrFiveHourUtilPct;
        result.fiveHourResetUnix = hdrFiveHourResetUnix;
    }
    if (hdrSevenDayUtilPct >= 0)
    {
        result.sevenDayUtilPct   = hdrSevenDayUtilPct;
        result.sevenDayResetUnix = hdrSevenDayResetUnix;
    }

    if (err != ESP_OK)
    {
        result.status = Status::NetworkError;
        ESP_LOGW(TAG, "Probe transport error: %s", esp_err_to_name(err));
    }
    else if (httpSts == 401 || httpSts == 403)
    {
        // Access token rejected — force a refresh on the next iteration.
        expiresAtUnix_ = 0;
        result.status  = Status::AuthError;
    }
    else if (httpSts == 429)
    {
        result.status = Status::RateLimited;
    }
    else if (httpSts >= 200 && httpSts < 300)
    {
        result.status        = Status::Ok;
        result.everSucceeded = true;
        result.updatedAt     = DateTime::Now();
    }
    else
    {
        result.status = Status::OtherError;
        ESP_LOGW(TAG, "Probe HTTP %d", httpSts);
    }

    {
        LOCK(mutex_);
        if (snapshot_.everSucceeded) result.everSucceeded = true;
        snapshot_ = result;
    }
    return result.status == Status::Ok;
}
