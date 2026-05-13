#include "DisplayPage.h"
#include "SettingsManager/SettingsManager.h"
#include "esp_system.h"

void DisplayPage::SaveAndReboot(SettingsManager &settings)
{
    settings.Save();
    ESP_LOGI("DisplayPage", "Settings saved, rebooting...");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}
