#include "HomePage.h"
#include "DateTime.h"
#include <cstdio>

void HomePage::OnCreate()
{
    lv_obj_set_style_bg_color(panel, lv_color_black(), LV_PART_MAIN);

    // Top bar: time on the left, IP on the right
    labelTime = lv_label_create(panel);
    lv_obj_set_pos(labelTime, 10, 6);
    lv_label_set_text(labelTime, "00:00:00");
    lv_obj_set_style_text_color(labelTime, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(labelTime, &lv_font_montserrat_14, LV_PART_MAIN);

    labelIP = lv_label_create(panel);
    lv_label_set_text(labelIP, "No IP");
    lv_obj_set_style_text_color(labelIP, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_font(labelIP, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(labelIP, LV_ALIGN_TOP_RIGHT, -60, 8);

    // Centered title placeholder — replace with meter readout when wired up
    char nameBuf[64] = {};
    if (!settingsManager.getString("device.name", nameBuf, sizeof(nameBuf)) || nameBuf[0] == '\0')
        snprintf(nameBuf, sizeof(nameBuf), "ClaudeMeter");

    labelTitle = lv_label_create(panel);
    lv_label_set_text(labelTitle, nameBuf);
    lv_obj_set_style_text_color(labelTitle, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(labelTitle, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_align(labelTitle, LV_ALIGN_CENTER, 0, -10);

    // Gear button → settings
    lv_obj_t *gearBtn = lv_btn_create(panel);
    lv_obj_set_size(gearBtn, 44, 26);
    lv_obj_align(gearBtn, LV_ALIGN_TOP_RIGHT, -4, 1);
    lv_obj_set_style_bg_color(gearBtn, lv_color_hex(0x282828), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(gearBtn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(gearBtn, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(gearBtn, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(gearBtn, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_radius(gearBtn, 4, LV_PART_MAIN);

    lv_obj_t *gearIcon = lv_label_create(gearBtn);
    lv_label_set_text(gearIcon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(gearIcon, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(gearIcon);
    lv_obj_add_event_cb(gearBtn, [](lv_event_t *e) {
        auto *page = static_cast<HomePage *>(lv_event_get_user_data(e));
        if (page->navigate)
            page->navigate("settings");
    }, LV_EVENT_CLICKED, this);
}

void HomePage::Update()
{
    char buf[32];

    DateTime now = DateTime::Now();
    now.ToStringLocal(buf, sizeof(buf), "%H:%M:%S");
    lv_label_set_text(labelTime, buf);

    auto status = networkManager.wifi().getStatus();
    if (status.has_ipv4)
        snprintf(buf, sizeof(buf), IPSTR, IP2STR(&status.ipv4.ip));
    else
        snprintf(buf, sizeof(buf), "No IP");
    lv_label_set_text(labelIP, buf);
}
