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
    task_.Init("ClaudeMeter", 5, 8192);
    task_.SetHandler([this]() { Work(); });
    task_.Run();

    init.SetReady();
    ESP_LOGI(TAG, "ClaudeMeterManager initialized.");
}

void ClaudeMeterManager::LoadConfig()
{
    auto &settings = serviceProvider_.getSettingsManager();
    apiKey_[0] = '\0';
    model_[0]  = '\0';
    settings.getString("claude.api_key", apiKey_, sizeof(apiKey_));
    settings.getString("claude.model",   model_,  sizeof(model_));
    if (model_[0] == '\0')
        snprintf(model_, sizeof(model_), "claude-haiku-4-5");
    pollSec_ = settings.getInt("claude.poll_s", 60);
    enabled_ = (apiKey_[0] != '\0') && (pollSec_ > 0);
}

void ClaudeMeterManager::Work()
{
    auto &network = serviceProvider_.getNetworkManager();

    while (true)
    {
        LoadConfig();

        if (!enabled_)
        {
            {
                LOCK(mutex_);
                snapshot_.status = Status::Unconfigured;
            }
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        if (!network.wifi().getStatus().has_ipv4)
        {
            {
                LOCK(mutex_);
                snapshot_.status = Status::WaitingForNetwork;
            }
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        PollOnce();
        vTaskDelay(pdMS_TO_TICKS(pollSec_ * 1000));
    }
}

void ClaudeMeterManager::PollOnce()
{
    inFlight_ = {};

    {
        LOCK(mutex_);
        snapshot_.status = Status::Polling;
    }

    esp_http_client_config_t cfg = {};
    cfg.url               = "https://api.anthropic.com/v1/messages";
    cfg.method            = HTTP_METHOD_POST;
    cfg.event_handler     = HttpEventHandler;
    cfg.user_data         = this;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.timeout_ms        = 15000;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "x-api-key", apiKey_);
    esp_http_client_set_header(client, "anthropic-version", "2023-06-01");
    esp_http_client_set_header(client, "content-type", "application/json");

    char body[192];
    int n = snprintf(body, sizeof(body),
        "{\"model\":\"%s\",\"max_tokens\":1,\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}",
        model_);
    esp_http_client_set_post_field(client, body, n);

    esp_err_t err     = esp_http_client_perform(client);
    int       httpSts = esp_http_client_get_status_code(client);

    Snapshot result = inFlight_;
    result.httpStatus = httpSts;

    if (err != ESP_OK)
    {
        result.status = Status::NetworkError;
        ESP_LOGW(TAG, "HTTP perform failed: %s", esp_err_to_name(err));
    }
    else if (httpSts == 401 || httpSts == 403)
    {
        result.status = Status::AuthError;
        ESP_LOGW(TAG, "Auth error (HTTP %d)", httpSts);
    }
    else if (httpSts == 429)
    {
        result.status = Status::RateLimited;
    }
    else if (httpSts >= 200 && httpSts < 300)
    {
        result.status      = Status::Ok;
        result.everSucceeded = true;
        result.fetchedAt   = DateTime::Now();
    }
    else
    {
        result.status = Status::OtherError;
        ESP_LOGW(TAG, "API error: HTTP %d", httpSts);
    }

    {
        LOCK(mutex_);
        if (snapshot_.everSucceeded)
            result.everSucceeded = true;
        snapshot_ = result;
    }

    esp_http_client_cleanup(client);
}

esp_err_t ClaudeMeterManager::HttpEventHandler(esp_http_client_event_t *evt)
{
    auto *self = static_cast<ClaudeMeterManager *>(evt->user_data);
    if (evt->event_id == HTTP_EVENT_ON_HEADER && self && evt->header_key && evt->header_value)
        self->HandleHeader(evt->header_key, evt->header_value);
    return ESP_OK;
}

void ClaudeMeterManager::HandleHeader(const char *key, const char *value)
{
    if      (strcasecmp(key, "anthropic-ratelimit-requests-limit")        == 0) inFlight_.reqLimit          = atoi(value);
    else if (strcasecmp(key, "anthropic-ratelimit-requests-remaining")    == 0) inFlight_.reqRemaining      = atoi(value);
    else if (strcasecmp(key, "anthropic-ratelimit-requests-reset")        == 0) inFlight_.reqResetInSecs    = ParseIsoToSecsFromNow(value);
    else if (strcasecmp(key, "anthropic-ratelimit-input-tokens-limit")    == 0) inFlight_.inTokLimit        = atoll(value);
    else if (strcasecmp(key, "anthropic-ratelimit-input-tokens-remaining")== 0) inFlight_.inTokRemaining    = atoll(value);
    else if (strcasecmp(key, "anthropic-ratelimit-input-tokens-reset")    == 0) inFlight_.inTokResetInSecs  = ParseIsoToSecsFromNow(value);
    else if (strcasecmp(key, "anthropic-ratelimit-output-tokens-limit")   == 0) inFlight_.outTokLimit       = atoll(value);
    else if (strcasecmp(key, "anthropic-ratelimit-output-tokens-remaining")==0) inFlight_.outTokRemaining   = atoll(value);
    else if (strcasecmp(key, "anthropic-ratelimit-output-tokens-reset")   == 0) inFlight_.outTokResetInSecs = ParseIsoToSecsFromNow(value);
}

int ClaudeMeterManager::ParseIsoToSecsFromNow(const char *iso)
{
    int y, m, d, hh, mm, ss;
    if (sscanf(iso, "%d-%d-%dT%d:%d:%dZ", &y, &m, &d, &hh, &mm, &ss) != 6)
        return -1;

    struct tm tmv = {};
    tmv.tm_year  = y - 1900;
    tmv.tm_mon   = m - 1;
    tmv.tm_mday  = d;
    tmv.tm_hour  = hh;
    tmv.tm_min   = mm;
    tmv.tm_sec   = ss;
    tmv.tm_isdst = 0;

    // Interpret the parsed fields as UTC by temporarily forcing TZ=UTC.
    char *tz = getenv("TZ");
    setenv("TZ", "UTC0", 1);
    tzset();
    time_t target = mktime(&tmv);
    if (tz) setenv("TZ", tz, 1); else unsetenv("TZ");
    tzset();

    if (target == (time_t)-1) return -1;
    time_t now  = time(nullptr);
    long   diff = (long)(target - now);
    if (diff < 0)     diff = 0;
    if (diff > 86400) diff = 86400;
    return (int)diff;
}

ClaudeMeterManager::Snapshot ClaudeMeterManager::GetSnapshot()
{
    LOCK(mutex_);
    return snapshot_;
}

const char *ClaudeMeterManager::StatusStr(Status s)
{
    switch (s)
    {
        case Status::Unconfigured:      return "Set claude.api_key";
        case Status::WaitingForNetwork: return "Waiting for network";
        case Status::Polling:           return "Polling...";
        case Status::Ok:                return "OK";
        case Status::AuthError:         return "Auth error";
        case Status::RateLimited:       return "Rate-limited (429)";
        case Status::NetworkError:      return "Network error";
        case Status::OtherError:        return "API error";
    }
    return "?";
}
