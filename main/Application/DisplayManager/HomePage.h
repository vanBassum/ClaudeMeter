#pragma once
#include "DisplayPage.h"
#include "NetworkManager/NetworkManager.h"
#include "SettingsManager/SettingsManager.h"
#include "ClaudeMeterManager/ClaudeMeterManager.h"

class HomePage : public DisplayPage
{
public:
    HomePage(NetworkManager &net, SettingsManager &settings, ClaudeMeterManager &meter)
        : networkManager(net), settingsManager(settings), claudeMeter(meter) {}

    void Update() override;

private:
    NetworkManager      &networkManager;
    SettingsManager     &settingsManager;
    ClaudeMeterManager  &claudeMeter;

    lv_obj_t *labelTime    = nullptr;
    lv_obj_t *labelIP      = nullptr;
    lv_obj_t *labelStatus  = nullptr;

    struct Row {
        lv_obj_t *title  = nullptr;
        lv_obj_t *bar    = nullptr;
        lv_obj_t *amount = nullptr;
        lv_obj_t *detail = nullptr;
    };
    Row rowIn;
    Row rowOut;
    Row rowCost;

    void OnCreate() override;
    void BuildRow(Row &row, const char *title, lv_color_t color, lv_coord_t y);
};
