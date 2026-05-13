#pragma once

#include "ServiceProvider.h"
#include "InitState.h"
#include "Task.h"
#include "Mutex.h"
#include "DateTime.h"
#include "esp_http_client.h"

class ClaudeMeterManager
{
    static constexpr const char *TAG = "ClaudeMeterManager";

public:
    enum class Status
    {
        Unconfigured,       // no api_key set or poll_s == 0
        WaitingForNetwork,  // wifi has no IP yet
        Polling,            // request in flight
        Ok,                 // last poll succeeded, rate-limit headers captured
        AuthError,          // 401/403
        RateLimited,        // 429
        NetworkError,       // tls/dns/connect failed
        OtherError          // 4xx/5xx
    };

    struct Snapshot
    {
        Status status = Status::Unconfigured;
        bool everSucceeded = false;
        DateTime fetchedAt{};
        int httpStatus = 0;

        // -1 means "unknown / header not seen"
        int     reqLimit          = -1;
        int     reqRemaining      = -1;
        int     reqResetInSecs    = -1;

        int64_t inTokLimit        = -1;
        int64_t inTokRemaining    = -1;
        int     inTokResetInSecs  = -1;

        int64_t outTokLimit       = -1;
        int64_t outTokRemaining   = -1;
        int     outTokResetInSecs = -1;
    };

    explicit ClaudeMeterManager(ServiceProvider &sp);
    ClaudeMeterManager(const ClaudeMeterManager &) = delete;
    ClaudeMeterManager &operator=(const ClaudeMeterManager &) = delete;

    void Init();

    Snapshot GetSnapshot();
    static const char *StatusStr(Status s);

private:
    ServiceProvider &serviceProvider_;
    InitState initState_;
    Task task_;
    Mutex mutex_;

    Snapshot snapshot_;
    Snapshot inFlight_;

    int  pollSec_ = 60;
    bool enabled_ = false;
    char apiKey_[80] = {};
    char model_[48] = {};

    void Work();
    void LoadConfig();
    void PollOnce();
    void HandleHeader(const char *key, const char *value);

    static esp_err_t HttpEventHandler(esp_http_client_event_t *evt);
    static int ParseIsoToSecsFromNow(const char *iso);
};
