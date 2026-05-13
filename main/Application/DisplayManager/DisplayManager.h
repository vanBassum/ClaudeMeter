#pragma once
#include "ServiceProvider.h"
#include "rtos.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "Display_WT32SC01.h"
#include "lvgl.h"

#include "HomePage.h"
#include "SettingsMenuPage.h"
#include "WifiPage.h"
#include "SystemPage.h"

class DisplayManager
{
    inline static constexpr const char *TAG = "DisplayManager";

public:
    explicit DisplayManager(ServiceProvider &ctx);
    void Init();

private:
    InitState initState;
    Display_WT32SC01 display;
    Task task;
    esp_timer_handle_t lvglTickTimer = nullptr;

    // Pages
    HomePage homePage;
    SettingsMenuPage settingsMenuPage;
    WifiPage wifiPage;
    SystemPage systemPage;
    DisplayPage *activePage = nullptr;

    void Work();
    static void LvglTickCb(void *arg);
    void NavigateTo(const char *page);
};
