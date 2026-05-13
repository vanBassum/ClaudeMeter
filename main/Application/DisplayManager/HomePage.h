#pragma once
#include "DisplayPage.h"
#include "NetworkManager/NetworkManager.h"
#include "SettingsManager/SettingsManager.h"

class HomePage : public DisplayPage
{
public:
    HomePage(NetworkManager &net, SettingsManager &settings)
        : networkManager(net), settingsManager(settings) {}

    void Update() override;

private:
    NetworkManager &networkManager;
    SettingsManager &settingsManager;

    lv_obj_t *labelTime = nullptr;
    lv_obj_t *labelIP = nullptr;
    lv_obj_t *labelTitle = nullptr;

    void OnCreate() override;
};
