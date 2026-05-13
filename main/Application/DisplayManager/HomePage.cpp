#include "HomePage.h"
#include "DateTime.h"
#include <cstdio>

void HomePage::OnCreate()
{
    lv_obj_set_style_bg_color(panel, lv_color_black(), LV_PART_MAIN);

    // Top bar
    labelTime = lv_label_create(panel);
    lv_obj_set_pos(labelTime, 10, 6);
    lv_label_set_text(labelTime, "00:00:00");
    lv_obj_set_style_text_color(labelTime, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(labelTime, &lv_font_montserrat_14, LV_PART_MAIN);

    labelStatus = lv_label_create(panel);
    lv_label_set_text(labelStatus, "Waiting for data");
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

    BuildRow(rowFiveHour, "Session (5hr)", 80);
    BuildRow(rowSevenDay, "Weekly (7 day)", 195);
}

void HomePage::BuildRow(Row &row, const char *title, lv_coord_t y)
{
    row.title = lv_label_create(panel);
    lv_label_set_text(row.title, title);
    lv_obj_set_pos(row.title, 18, y);
    lv_obj_set_style_text_color(row.title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(row.title, &lv_font_montserrat_20, LV_PART_MAIN);

    row.percent = lv_label_create(panel);
    lv_label_set_text(row.percent, "--%");
    lv_obj_set_style_text_color(row.percent, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(row.percent, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(row.percent, LV_ALIGN_TOP_LEFT, LCD_HRES - 80, y);

    row.bar = lv_bar_create(panel);
    lv_obj_set_size(row.bar, LCD_HRES - 36, 14);
    lv_obj_set_pos(row.bar, 18, y + 34);
    lv_bar_set_range(row.bar, 0, 100);
    lv_bar_set_value(row.bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(row.bar, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(row.bar, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_border_width(row.bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(row.bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(row.bar, 2, LV_PART_INDICATOR);

    row.reset = lv_label_create(panel);
    lv_label_set_text(row.reset, "");
    lv_obj_set_style_text_color(row.reset, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_font(row.reset, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(row.reset, 18, y + 56);
}

static void FormatResetIn(char *buf, size_t n, int64_t secsUntilReset)
{
    if (secsUntilReset <= 0) {
        snprintf(buf, n, "Resets now");
        return;
    }
    if (secsUntilReset < 3600)
        snprintf(buf, n, "Resets in %dm", (int)(secsUntilReset / 60));
    else if (secsUntilReset < 86400)
        snprintf(buf, n, "Resets in %dh", (int)(secsUntilReset / 3600));
    else
        snprintf(buf, n, "Resets in %dd", (int)(secsUntilReset / 86400));
}

void HomePage::UpdateRow(Row &row, int utilPct, int64_t resetUnix, DateTime now)
{
    if (utilPct < 0)
    {
        lv_label_set_text(row.percent, "--%");
        lv_label_set_text(row.reset,   "");
        lv_bar_set_value(row.bar, 0, LV_ANIM_OFF);
        return;
    }

    char buf[24];
    snprintf(buf, sizeof(buf), "%d%%", utilPct);
    lv_label_set_text(row.percent, buf);
    lv_bar_set_value(row.bar, utilPct > 100 ? 100 : utilPct, LV_ANIM_OFF);

    // Colour the bar by severity, matching the screenshot's blue-then-orange-then-red feel
    lv_color_t color;
    if (utilPct >= 90)      color = lv_palette_main(LV_PALETTE_RED);
    else if (utilPct >= 75) color = lv_palette_main(LV_PALETTE_ORANGE);
    else                    color = lv_palette_main(LV_PALETTE_BLUE);
    lv_obj_set_style_bg_color(row.bar, color, LV_PART_INDICATOR);

    if (resetUnix > 0)
    {
        int64_t secsLeft = resetUnix - (int64_t)now.UtcSeconds();
        FormatResetIn(buf, sizeof(buf), secsLeft);
        lv_label_set_text(row.reset, buf);
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

    using S = ClaudeMeterManager::Status;
    lv_color_t statusColor = lv_palette_main(LV_PALETTE_GREY);
    char statusBuf[48];

    if (snap.status == S::Ok)
    {
        TimeSpan age = now - snap.updatedAt;
        int ageSecs = (int)age.TotalSeconds();
        if (ageSecs < 0) ageSecs = 0;
        if (ageSecs < 60)
            snprintf(statusBuf, sizeof(statusBuf), "Updated %ds ago", ageSecs);
        else if (ageSecs < 3600)
            snprintf(statusBuf, sizeof(statusBuf), "Updated %dm ago", ageSecs / 60);
        else
            snprintf(statusBuf, sizeof(statusBuf), "Updated %dh ago", ageSecs / 3600);
        statusColor = (ageSecs < (int)(2 * 60))
                          ? lv_palette_main(LV_PALETTE_GREEN)
                          : lv_palette_main(LV_PALETTE_GREY);
    }
    else
    {
        snprintf(statusBuf, sizeof(statusBuf), "%s", ClaudeMeterManager::StatusStr(snap.status));
        switch (snap.status)
        {
            case S::Refreshing:
            case S::Probing:       statusColor = lv_palette_main(LV_PALETTE_BLUE);   break;
            case S::RateLimited:   statusColor = lv_palette_main(LV_PALETTE_ORANGE); break;
            case S::AuthError:
            case S::NetworkError:
            case S::OtherError:    statusColor = lv_palette_main(LV_PALETTE_RED);    break;
            default:               statusColor = lv_palette_main(LV_PALETTE_GREY);   break;
        }
    }
    lv_label_set_text(labelStatus, statusBuf);
    lv_obj_set_style_text_color(labelStatus, statusColor, LV_PART_MAIN);

    // Render whatever utilisation we last captured (the manager preserves the
    // previous values across non-OK polls so the bars don't flicker).
    UpdateRow(rowFiveHour, snap.fiveHourUtilPct, snap.fiveHourResetUnix, now);
    UpdateRow(rowSevenDay, snap.sevenDayUtilPct, snap.sevenDayResetUnix, now);
}
