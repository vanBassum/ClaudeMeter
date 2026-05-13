#pragma once

#include "ServiceProvider.h"
#include "InitState.h"
#include "Mutex.h"
#include "DateTime.h"
#include <cstdint>

// Mirrors the data Claude Code's /usage panel shows. A companion script on
// the developer's machine probes Anthropic with the local OAuth token (using
// the same beta header Claude Code uses), reads the
// `anthropic-ratelimit-unified-*` response headers, and POSTs the digest to
// /api/usage on this device.
class ClaudeMeterManager
{
    static constexpr const char *TAG = "ClaudeMeterManager";

public:
    struct Snapshot
    {
        bool       valid               = false;
        DateTime   updatedAt{};

        // Each utilization is 0..100, or -1 if the field wasn't seen.
        int        fiveHourUtilPct     = -1;
        int64_t    fiveHourResetUnix   = 0;

        int        sevenDayUtilPct     = -1;
        int64_t    sevenDayResetUnix   = 0;
    };

    explicit ClaudeMeterManager(ServiceProvider &sp);
    ClaudeMeterManager(const ClaudeMeterManager &) = delete;
    ClaudeMeterManager &operator=(const ClaudeMeterManager &) = delete;

    void Init();

    Snapshot GetSnapshot();

    // Returns an HTTP status code (200, 400, 401) and writes a short response
    // string. Body is the raw POST body; bearer is the value of the
    // Authorization header (or nullptr if not provided).
    int Ingest(const char *body, size_t bodyLen,
               const char *bearer,
               char *outResp, size_t outRespCap);

private:
    ServiceProvider &serviceProvider_;
    InitState        initState_;
    Mutex            mutex_;
    Snapshot         snapshot_;
};
