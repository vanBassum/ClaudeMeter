#include "DisplayManager.h"
#include <algorithm>
#include <cstring>

DisplayManager::DisplayManager(ServiceProvider &ctx)
    : homePage(ctx.getNetworkManager(), ctx.getSettingsManager())
    , wifiPage(ctx.getSettingsManager(), ctx.getNetworkManager())
    , systemPage(ctx.getSettingsManager())
{
    auto nav = [this](const char *page) { NavigateTo(page); };
    homePage.SetNavigator(nav);
    settingsMenuPage.SetNavigator(nav);
    wifiPage.SetNavigator(nav);
    systemPage.SetNavigator(nav);
}

void DisplayManager::Init()
{
    auto init = initState.TryBeginInit();
    if (!init)
        return;

    ESP_LOGI(TAG, "Initializing DisplayManager...");

    lv_init();
    display.Init();

    const esp_timer_create_args_t tickTimerArgs = {
        .callback = LvglTickCb,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "LvglTick",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&tickTimerArgs, &lvglTickTimer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvglTickTimer, 5000));

    task.Init("DisplayTask", 5, 4096);
    task.SetHandler([this]() { Work(); });
    task.Run();

    init.SetReady();
    ESP_LOGI(TAG, "DisplayManager initialized successfully.");
}

void DisplayManager::Work()
{
    NavigateTo("home");

    uint32_t delayMs;
    TickType_t lastUpdate = xTaskGetTickCount();

    while (true)
    {
        delayMs = lv_timer_handler();

        if (xTaskGetTickCount() - lastUpdate > pdMS_TO_TICKS(1000))
        {
            lastUpdate = xTaskGetTickCount();

            if (activePage)
                activePage->Update();
        }

        vTaskDelay(pdMS_TO_TICKS(std::clamp(delayMs, (uint32_t)5, (uint32_t)100)));
    }
}

void DisplayManager::LvglTickCb(void *arg)
{
    (void)arg;
    lv_tick_inc(5);
}

void DisplayManager::NavigateTo(const char *page)
{
    DisplayPage *previousPage = activePage;

    if (activePage)
    {
        activePage->Hide();
        activePage = nullptr;
    }

    if (strcmp(page, "back") == 0)
    {
        // Back from sub-pages → settings menu, back from menu → home
        if (previousPage == &settingsMenuPage)
            activePage = &homePage;
        else
            activePage = &settingsMenuPage;
    }
    else if (strcmp(page, "home") == 0)
        activePage = &homePage;
    else if (strcmp(page, "settings") == 0)
        activePage = &settingsMenuPage;
    else if (strcmp(page, "wifi") == 0)
        activePage = &wifiPage;
    else if (strcmp(page, "system") == 0)
        activePage = &systemPage;
    else
        activePage = &homePage;

    if (activePage)
        activePage->Show(lv_scr_act());
}
