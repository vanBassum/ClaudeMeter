#pragma once
#include "DisplayPage.h"

class SettingsManager;

class SystemPage : public DisplayPage
{
public:
    SystemPage(SettingsManager &settings) : settingsManager(settings) {}

private:
    SettingsManager &settingsManager;

    void OnCreate() override;

    static void SaveCb(lv_event_t *e);
};
