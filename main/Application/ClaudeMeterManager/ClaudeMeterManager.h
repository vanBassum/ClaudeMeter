#pragma once

#include "ServiceProvider.h"
#include "InitState.h"
#include "Mutex.h"
#include "DateTime.h"
#include <cstdint>

// Receives usage aggregates pushed from a companion script on the developer's
// machine that scrapes ~/.claude/projects/**/*.jsonl. ClaudeMeterManager does
// not initiate any outbound traffic — it only caches the latest snapshot for
// the display.
class ClaudeMeterManager
{
    static constexpr const char *TAG = "ClaudeMeterManager";

public:
    struct Snapshot
    {
        bool       valid               = false;
        DateTime   updatedAt{};

        // Totals attributed to "today" by the scraper.
        int64_t    inputTokensToday    = 0;
        int64_t    outputTokensToday   = 0;
        int64_t    cacheCreationToday  = 0;
        int64_t    cacheReadToday      = 0;
        int64_t    costCentsToday      = 0;

        // When the most recent Claude Code turn happened (seconds since epoch,
        // 0 if unknown).
        int64_t    lastActivityUnix    = 0;
    };

    explicit ClaudeMeterManager(ServiceProvider &sp);
    ClaudeMeterManager(const ClaudeMeterManager &) = delete;
    ClaudeMeterManager &operator=(const ClaudeMeterManager &) = delete;

    void Init();

    Snapshot GetSnapshot();

    // Returns an HTTP status code (200, 400, 401) and writes a short response
    // string to outResp/outRespCap suitable for httpd_resp_send. Body is the
    // raw POST body; bearer is the value of the Authorization header (or
    // nullptr if not provided).
    int Ingest(const char *body, size_t bodyLen,
               const char *bearer,
               char *outResp, size_t outRespCap);

private:
    ServiceProvider &serviceProvider_;
    InitState        initState_;
    Mutex            mutex_;
    Snapshot         snapshot_;
};
