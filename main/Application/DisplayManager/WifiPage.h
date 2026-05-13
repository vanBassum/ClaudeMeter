#pragma once
#include "DisplayPage.h"

class SettingsManager;
class NetworkManager;

class WifiPage : public DisplayPage
{
public:
    WifiPage(SettingsManager &settings, NetworkManager &network)
        : settingsManager(settings), networkManager(network) {}

private:
    SettingsManager &settingsManager;
    NetworkManager &networkManager;

    lv_obj_t *listArea = nullptr;
    lv_obj_t *passwordTa = nullptr;

    void OnCreate() override;
    void RunScan();
    void SelectNetwork(const char *ssid);

    static void ScanCb(lv_event_t *e);
    static void NetworkCb(lv_event_t *e);
    static void SaveCb(lv_event_t *e);
};
