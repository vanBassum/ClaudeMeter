#include "WifiPage.h"
#include "SettingsManager/SettingsManager.h"
#include "NetworkManager/NetworkManager.h"
#include <cstdio>
#include <cstring>

void WifiPage::OnCreate()
{
    AddTopBar(LV_SYMBOL_WIFI " WiFi");

    // Scan button
    lv_obj_t *scanBtn = AddButton(LV_SYMBOL_REFRESH " Scan",
                                   lv_palette_main(LV_PALETTE_BLUE), 80, 30, ScanCb);
    lv_obj_set_pos(scanBtn, LCD_HRES - 95, 6);

    // Network list
    listArea = lv_obj_create(panel);
    lv_obj_set_size(listArea, LCD_HRES - 20, 130);
    lv_obj_set_pos(listArea, 10, 44);
    lv_obj_set_style_bg_color(listArea, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    lv_obj_set_style_border_color(listArea, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_border_width(listArea, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(listArea, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(listArea, 4, LV_PART_MAIN);
    lv_obj_set_flex_flow(listArea, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(listArea, 2, LV_PART_MAIN);

    lv_obj_t *hint = lv_label_create(listArea);
    lv_label_set_text(hint, "Tap 'Scan' to find networks");
    lv_obj_set_style_text_color(hint, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);

    // Password
    char passBuf[65] = {};
    settingsManager.getString("wifi.password", passBuf, sizeof(passBuf));
    passwordTa = AddTextRow(LV_SYMBOL_EYE_CLOSE " Pass", passBuf, 180, 64, true);

    // Save & Reboot
    lv_obj_t *saveBtn = AddButton(LV_SYMBOL_OK " Save & Reboot",
                                   lv_palette_main(LV_PALETTE_BLUE), 180, 38, SaveCb);
    lv_obj_set_pos(saveBtn, LCD_HRES - 195, 222);

    RunScan();
}

void WifiPage::RunScan()
{
    lv_obj_clean(listArea);

    lv_obj_t *scanning = lv_label_create(listArea);
    lv_label_set_text(scanning, "Scanning...");
    lv_obj_set_style_text_color(scanning, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_refr_now(NULL);

    WiFiInterface::ScanResult results[15] = {};
    int count = networkManager.wifi().Scan(results, 15);

    lv_obj_clean(listArea);

    if (count == 0)
    {
        lv_obj_t *lbl = lv_label_create(listArea);
        lv_label_set_text(lbl, "No networks found");
        lv_obj_set_style_text_color(lbl, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        return;
    }

    char currentSsid[33] = {};
    settingsManager.getString("wifi.ssid", currentSsid, sizeof(currentSsid));

    for (int i = 0; i < count; i++)
    {
        lv_obj_t *row = lv_btn_create(listArea);
        lv_obj_set_size(row, lv_pct(100), 28);
        lv_obj_set_style_radius(row, 4, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(row, 8, LV_PART_MAIN);

        bool isActive = strcmp(results[i].ssid, currentSsid) == 0;
        lv_obj_set_style_bg_color(row, isActive ? lv_palette_main(LV_PALETTE_BLUE)
                                                : lv_color_hex(0x2a2a2a), LV_PART_MAIN);

        char rowText[48];
        snprintf(rowText, sizeof(rowText), "%s%s  (%ddBm)",
                 results[i].secure ? LV_SYMBOL_EYE_CLOSE " " : "",
                 results[i].ssid, results[i].rssi);

        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, rowText);
        lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_add_event_cb(row, NetworkCb, LV_EVENT_CLICKED, this);
    }
}

void WifiPage::SelectNetwork(const char *ssid)
{
    settingsManager.setString("wifi.ssid", ssid);

    // Update row highlights without rescanning
    uint32_t count = lv_obj_get_child_cnt(listArea);
    for (uint32_t i = 0; i < count; i++)
    {
        lv_obj_t *row = lv_obj_get_child(listArea, i);
        if (!lv_obj_check_type(row, &lv_btn_class))
            continue;

        lv_obj_t *lbl = lv_obj_get_child(row, 0);
        if (!lbl)
            continue;

        const char *text = lv_label_get_text(lbl);
        bool match = strstr(text, ssid) != nullptr;
        lv_obj_set_style_bg_color(row, match ? lv_palette_main(LV_PALETTE_BLUE)
                                             : lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    }
}

void WifiPage::ScanCb(lv_event_t *e)
{
    static_cast<WifiPage *>(lv_event_get_user_data(e))->RunScan();
}

void WifiPage::NetworkCb(lv_event_t *e)
{
    auto *self = static_cast<WifiPage *>(lv_event_get_user_data(e));
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    if (!lbl)
        return;

    // Extract SSID: skip optional lock symbol, find "  (" delimiter
    const char *text = lv_label_get_text(lbl);
    const char *ssidStart = text;

    // Skip LV_SYMBOL prefix (3-byte UTF-8 + space)
    if ((uint8_t)ssidStart[0] == 0xEF)
    {
        ssidStart = strchr(text, ' ');
        if (ssidStart)
            ssidStart++;
        else
            ssidStart = text;
    }

    const char *end = strstr(ssidStart, "  (");
    if (!end)
        return;

    char ssid[33] = {};
    int len = end - ssidStart;
    if (len > 32) len = 32;
    strncpy(ssid, ssidStart, len);

    self->SelectNetwork(ssid);
}

void WifiPage::SaveCb(lv_event_t *e)
{
    auto *self = static_cast<WifiPage *>(lv_event_get_user_data(e));
    const char *pass = lv_textarea_get_text(self->passwordTa);
    self->settingsManager.setString("wifi.password", pass);
    self->SaveAndReboot(self->settingsManager);
}
