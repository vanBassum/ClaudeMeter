#include "ClaudeMeterManager.h"
#include "SettingsManager/SettingsManager.h"
#include "ContextLock.h"
#include "esp_log.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>

ClaudeMeterManager::ClaudeMeterManager(ServiceProvider &sp)
    : serviceProvider_(sp)
{
}

void ClaudeMeterManager::Init()
{
    auto init = initState_.TryBeginInit();
    if (!init) return;
    ESP_LOGI(TAG, "Ready as inbound usage sink (POST /api/usage)");
    init.SetReady();
}

ClaudeMeterManager::Snapshot ClaudeMeterManager::GetSnapshot()
{
    LOCK(mutex_);
    return snapshot_;
}

// Minimal JSON int extractor. Finds `"key" : <number>` anywhere in `json` and
// returns the parsed integer; returns 0 if the key isn't present.
//
// Good enough because: (a) we control the producer (scrape_claude_usage.py),
// (b) the schema is flat with no naming collisions, (c) all values we care
// about are integers, and (d) ESP-IDF v6 removed the bundled `json` component.
static int64_t json_int(const char *json, const char *key)
{
    char needle[48];
    int n = snprintf(needle, sizeof(needle), "\"%s\"", key);
    if (n < 0 || n >= (int)sizeof(needle)) return 0;

    const char *p = strstr(json, needle);
    if (!p) return 0;
    p += n;

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != ':') return 0;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

    // strtoll handles optional leading '-' and stops at non-digit.
    char *end = nullptr;
    long long v = strtoll(p, &end, 10);
    if (end == p) return 0;
    return (int64_t)v;
}

int ClaudeMeterManager::Ingest(const char *body, size_t bodyLen,
                               const char *bearer,
                               char *outResp, size_t outRespCap)
{
    // Auth: if usage.auth_tok is set, the request must present the same token
    // as a bearer. If the setting is empty, the endpoint is unauthed (LAN-only
    // deployments).
    auto &settings = serviceProvider_.getSettingsManager();
    char expectedToken[64] = {};
    settings.getString("usage.auth_tok", expectedToken, sizeof(expectedToken));
    if (expectedToken[0] != '\0')
    {
        if (!bearer || strncmp(bearer, "Bearer ", 7) != 0 ||
            strcmp(bearer + 7, expectedToken) != 0)
        {
            snprintf(outResp, outRespCap, "{\"ok\":false,\"err\":\"unauthorized\"}");
            return 401;
        }
    }

    if (bodyLen >= 4096)
    {
        snprintf(outResp, outRespCap, "{\"ok\":false,\"err\":\"body_too_large\"}");
        return 400;
    }
    char buf[4096];
    memcpy(buf, body, bodyLen);
    buf[bodyLen] = '\0';

    // Sanity check: looks like a JSON object.
    const char *firstNonWs = buf;
    while (*firstNonWs && isspace((unsigned char)*firstNonWs)) firstNonWs++;
    if (*firstNonWs != '{')
    {
        snprintf(outResp, outRespCap, "{\"ok\":false,\"err\":\"bad_json\"}");
        return 400;
    }

    Snapshot s;
    s.inputTokensToday   = json_int(buf, "input_tokens");
    s.outputTokensToday  = json_int(buf, "output_tokens");
    s.cacheCreationToday = json_int(buf, "cache_creation_tokens");
    s.cacheReadToday     = json_int(buf, "cache_read_tokens");
    s.costCentsToday     = json_int(buf, "cost_cents");
    s.lastActivityUnix   = json_int(buf, "last_activity_unix");
    s.valid              = true;
    s.updatedAt          = DateTime::Now();

    {
        LOCK(mutex_);
        snapshot_ = s;
    }

    snprintf(outResp, outRespCap, "{\"ok\":true}");
    return 200;
}
