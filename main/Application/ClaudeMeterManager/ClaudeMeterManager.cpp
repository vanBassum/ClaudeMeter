#include "ClaudeMeterManager.h"
#include "SettingsManager/SettingsManager.h"
#include "ContextLock.h"
#include "esp_log.h"
#include "cJSON.h"
#include <cstring>
#include <cstdio>

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

static int64_t json_int(const cJSON *parent, const char *key)
{
    if (!parent) return 0;
    const cJSON *node = cJSON_GetObjectItemCaseSensitive(parent, key);
    if (cJSON_IsNumber(node)) return (int64_t)node->valuedouble;
    return 0;
}

int ClaudeMeterManager::Ingest(const char *body, size_t bodyLen,
                               const char *bearer,
                               char *outResp, size_t outRespCap)
{
    // Auth: if usage.ingest_token is set, the request must present the same
    // token as a bearer. If the setting is empty, the endpoint is unauthed
    // (LAN-only deployments).
    auto &settings = serviceProvider_.getSettingsManager();
    char expectedToken[64] = {};
    settings.getString("usage.ingest_token", expectedToken, sizeof(expectedToken));
    if (expectedToken[0] != '\0')
    {
        if (!bearer || strncmp(bearer, "Bearer ", 7) != 0 ||
            strcmp(bearer + 7, expectedToken) != 0)
        {
            snprintf(outResp, outRespCap, "{\"ok\":false,\"err\":\"unauthorized\"}");
            return 401;
        }
    }

    // We need a null-terminated string for cJSON.
    if (bodyLen >= 4096)
    {
        snprintf(outResp, outRespCap, "{\"ok\":false,\"err\":\"body_too_large\"}");
        return 400;
    }
    char buf[4096];
    memcpy(buf, body, bodyLen);
    buf[bodyLen] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root)
    {
        snprintf(outResp, outRespCap, "{\"ok\":false,\"err\":\"bad_json\"}");
        return 400;
    }

    Snapshot s;
    const cJSON *today = cJSON_GetObjectItemCaseSensitive(root, "today");
    s.inputTokensToday   = json_int(today, "input_tokens");
    s.outputTokensToday  = json_int(today, "output_tokens");
    s.cacheCreationToday = json_int(today, "cache_creation_tokens");
    s.cacheReadToday     = json_int(today, "cache_read_tokens");
    s.costCentsToday     = json_int(today, "cost_cents");
    s.lastActivityUnix   = json_int(root,  "last_activity_unix");
    s.valid              = true;
    s.updatedAt          = DateTime::Now();

    {
        LOCK(mutex_);
        snapshot_ = s;
    }

    cJSON_Delete(root);
    snprintf(outResp, outRespCap, "{\"ok\":true}");
    return 200;
}
