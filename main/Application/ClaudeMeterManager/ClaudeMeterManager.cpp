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
// returns the parsed integer; returns `missing` if the key isn't present.
//
// Good enough because: the producer is our scraper, the schema is flat with
// no key collisions, and all values we care about are integers.
static int64_t json_int(const char *json, const char *key, int64_t missing = 0)
{
    char needle[48];
    int n = snprintf(needle, sizeof(needle), "\"%s\"", key);
    if (n < 0 || n >= (int)sizeof(needle)) return missing;

    const char *p = strstr(json, needle);
    if (!p) return missing;
    p += n;

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != ':') return missing;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

    char *end = nullptr;
    long long v = strtoll(p, &end, 10);
    if (end == p) return missing;
    return (int64_t)v;
}

int ClaudeMeterManager::Ingest(const char *body, size_t bodyLen,
                               const char *bearer,
                               char *outResp, size_t outRespCap)
{
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

    const char *firstNonWs = buf;
    while (*firstNonWs && isspace((unsigned char)*firstNonWs)) firstNonWs++;
    if (*firstNonWs != '{')
    {
        snprintf(outResp, outRespCap, "{\"ok\":false,\"err\":\"bad_json\"}");
        return 400;
    }

    Snapshot s;
    s.fiveHourUtilPct   = (int)json_int(buf, "five_hour_util",  -1);
    s.fiveHourResetUnix =       json_int(buf, "five_hour_reset",  0);
    s.sevenDayUtilPct   = (int)json_int(buf, "seven_day_util",  -1);
    s.sevenDayResetUnix =       json_int(buf, "seven_day_reset",  0);
    s.valid             = (s.fiveHourUtilPct >= 0 || s.sevenDayUtilPct >= 0);
    s.updatedAt         = DateTime::Now();

    {
        LOCK(mutex_);
        snapshot_ = s;
    }

    snprintf(outResp, outRespCap, "{\"ok\":true}");
    return 200;
}
