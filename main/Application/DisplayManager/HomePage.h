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

    // Top bar
    lv_obj_t *labelTime    = nullptr;
    lv_obj_t *labelIP      = nullptr;
    lv_obj_t *labelStatus  = nullptr;

    // Three rate-limit rows: label, bar, "remaining / limit", reset countdown
    struct Row {
        lv_obj_t *title   = nullptr;
        lv_obj_t *bar     = nullptr;
        lv_obj_t *amount  = nullptr;
        lv_obj_t *reset   = nullptr;
    };
    Row rowReq;
    Row rowInTok;
    Row rowOutTok;

    void OnCreate() override;
    void BuildRow(Row &row, const char *title, lv_color_t color, lv_coord_t y);
    static void UpdateRow(Row &row, int64_t remaining, int64_t limit, int resetSecs, const char *unit);
};
