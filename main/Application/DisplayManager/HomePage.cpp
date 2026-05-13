#include "HomePage.h"
#include "DateTime.h"
#include <cstdio>

void HomePage::OnCreate()
{
    lv_obj_set_style_bg_color(panel, lv_color_black(), LV_PART_MAIN);

    // Top bar: time, IP, status
    labelTime = lv_label_create(panel);
    lv_obj_set_pos(labelTime, 10, 6);
    lv_label_set_text(labelTime, "00:00:00");
    lv_obj_set_style_text_color(labelTime, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(labelTime, &lv_font_montserrat_14, LV_PART_MAIN);

    labelStatus = lv_label_create(panel);
    lv_label_set_text(labelStatus, "—");
    lv_obj_set_style_text_color(labelStatus, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_font(labelStatus, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(labelStatus, LV_ALIGN_TOP_MID, 0, 8);

    labelIP = lv_label_create(panel);
    lv_label_set_text(labelIP, "No IP");
    lv_obj_set_style_text_color(labelIP, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_font(labelIP, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(labelIP, LV_ALIGN_TOP_RIGHT, -54, 8);

    // Gear → settings
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

    // Three rate-limit rows
    BuildRow(rowReq,    "Requests",      lv_palette_main(LV_PALETTE_BLUE),   50);
    BuildRow(rowInTok,  "Input tokens",  lv_palette_main(LV_PALETTE_GREEN), 140);
    BuildRow(rowOutTok, "Output tokens", lv_palette_main(LV_PALETTE_ORANGE), 230);
}

void HomePage::BuildRow(Row &row, const char *title, lv_color_t color, lv_coord_t y)
{
    row.title = lv_label_create(panel);
    lv_label_set_text(row.title, title);
    lv_obj_set_pos(row.title, 14, y);
    lv_obj_set_style_text_color(row.title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(row.title, &lv_font_montserrat_20, LV_PART_MAIN);

    row.reset = lv_label_create(panel);
    lv_label_set_text(row.reset, "");
    lv_obj_set_style_text_color(row.reset, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_font(row.reset, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(row.reset, LCD_HRES - 130, y + 4);

    row.bar = lv_bar_create(panel);
    lv_obj_set_size(row.bar, LCD_HRES - 28, 18);
    lv_obj_set_pos(row.bar, 14, y + 32);
    lv_bar_set_range(row.bar, 0, 1000);
    lv_bar_set_value(row.bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(row.bar, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(row.bar, color, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(row.bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(row.bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_radius(row.bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(row.bar, 3, LV_PART_INDICATOR);

    row.amount = lv_label_create(panel);
    lv_label_set_text(row.amount, "—");
    lv_obj_set_style_text_color(row.amount, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_font(row.amount, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(row.amount, 14, y + 54);
}

static void FormatNumber(char *buf, size_t n, int64_t v)
{
    if (v < 0) { snprintf(buf, n, "?"); return; }
    if (v < 1000)            snprintf(buf, n, "%lld",   (long long)v);
    else if (v < 1000000)    snprintf(buf, n, "%.1fk",  v / 1000.0);
    else                     snprintf(buf, n, "%.2fM",  v / 1000000.0);
}

void HomePage::UpdateRow(Row &row, int64_t remaining, int64_t limit, int resetSecs, const char *unit)
{
    char remBuf[16], limBuf[16], amountBuf[48];
    FormatNumber(remBuf, sizeof(remBuf), remaining);
    FormatNumber(limBuf, sizeof(limBuf), limit);
    snprintf(amountBuf, sizeof(amountBuf), "%s / %s %s", remBuf, limBuf, unit);
    lv_label_set_text(row.amount, amountBuf);

    int32_t pct = 0;
    if (limit > 0 && remaining >= 0)
    {
        pct = (int32_t)((remaining * 1000) / limit);
        if (pct < 0) pct = 0;
        if (pct > 1000) pct = 1000;
    }
    lv_bar_set_value(row.bar, pct, LV_ANIM_OFF);

    if (resetSecs >= 0)
    {
        char resetBuf[32];
        if (resetSecs < 60)
            snprintf(resetBuf, sizeof(resetBuf), "reset %ds", resetSecs);
        else if (resetSecs < 3600)
            snprintf(resetBuf, sizeof(resetBuf), "reset %dm %ds", resetSecs / 60, resetSecs % 60);
        else
            snprintf(resetBuf, sizeof(resetBuf), "reset %dh %dm", resetSecs / 3600, (resetSecs % 3600) / 60);
        lv_label_set_text(row.reset, resetBuf);
    }
    else
    {
        lv_label_set_text(row.reset, "");
    }
}

void HomePage::Update()
{
    char buf[40];

    DateTime now = DateTime::Now();
    now.ToStringLocal(buf, sizeof(buf), "%H:%M:%S");
    lv_label_set_text(labelTime, buf);

    auto status = networkManager.wifi().getStatus();
    if (status.has_ipv4)
        snprintf(buf, sizeof(buf), IPSTR, IP2STR(&status.ipv4.ip));
    else
        snprintf(buf, sizeof(buf), "No IP");
    lv_label_set_text(labelIP, buf);

    auto snap = claudeMeter.GetSnapshot();
    lv_label_set_text(labelStatus, ClaudeMeterManager::StatusStr(snap.status));

    // Colour the centre status by severity
    lv_color_t statusColor = lv_palette_main(LV_PALETTE_GREY);
    switch (snap.status)
    {
        case ClaudeMeterManager::Status::Ok:           statusColor = lv_palette_main(LV_PALETTE_GREEN); break;
        case ClaudeMeterManager::Status::Polling:      statusColor = lv_palette_main(LV_PALETTE_BLUE);  break;
        case ClaudeMeterManager::Status::RateLimited:  statusColor = lv_palette_main(LV_PALETTE_ORANGE); break;
        case ClaudeMeterManager::Status::AuthError:
        case ClaudeMeterManager::Status::NetworkError:
        case ClaudeMeterManager::Status::OtherError:   statusColor = lv_palette_main(LV_PALETTE_RED);   break;
        default: break;
    }
    lv_obj_set_style_text_color(labelStatus, statusColor, LV_PART_MAIN);

    UpdateRow(rowReq,    snap.reqRemaining,    snap.reqLimit,    snap.reqResetInSecs,    "req");
    UpdateRow(rowInTok,  snap.inTokRemaining,  snap.inTokLimit,  snap.inTokResetInSecs,  "in");
    UpdateRow(rowOutTok, snap.outTokRemaining, snap.outTokLimit, snap.outTokResetInSecs, "out");
}
