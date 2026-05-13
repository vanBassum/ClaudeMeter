#include "HomePage.h"
#include "DateTime.h"
#include <cstdio>

void HomePage::OnCreate()
{
    lv_obj_set_style_bg_color(panel, lv_color_black(), LV_PART_MAIN);

    // Top bar: time, status, IP
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

    BuildRow(rowIn,   "Input",  lv_palette_main(LV_PALETTE_GREEN),  50);
    BuildRow(rowOut,  "Output", lv_palette_main(LV_PALETTE_ORANGE),140);
    BuildRow(rowCost, "Cost",   lv_palette_main(LV_PALETTE_RED),   230);
}

void HomePage::BuildRow(Row &row, const char *title, lv_color_t color, lv_coord_t y)
{
    row.title = lv_label_create(panel);
    lv_label_set_text(row.title, title);
    lv_obj_set_pos(row.title, 14, y);
    lv_obj_set_style_text_color(row.title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(row.title, &lv_font_montserrat_20, LV_PART_MAIN);

    row.detail = lv_label_create(panel);
    lv_label_set_text(row.detail, "");
    lv_obj_set_style_text_color(row.detail, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_font(row.detail, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(row.detail, LCD_HRES - 180, y + 4);

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

static void FormatTokens(char *buf, size_t n, int64_t v)
{
    if (v < 0)             snprintf(buf, n, "?");
    else if (v < 1000)     snprintf(buf, n, "%lld",   (long long)v);
    else if (v < 1000000)  snprintf(buf, n, "%.1fk",  v / 1000.0);
    else                   snprintf(buf, n, "%.2fM",  v / 1000000.0);
}

static void FormatCost(char *buf, size_t n, int64_t cents)
{
    if (cents < 0) { snprintf(buf, n, "?"); return; }
    snprintf(buf, n, "$%lld.%02lld", (long long)(cents / 100), (long long)(cents % 100));
}

static void SetBarPct(lv_obj_t *bar, int64_t used, int64_t budget)
{
    int32_t pct = 0;
    if (budget > 0 && used >= 0)
    {
        pct = (int32_t)((used * 1000) / budget);
        if (pct < 0)    pct = 0;
        if (pct > 1000) pct = 1000;
    }
    lv_bar_set_value(bar, pct, LV_ANIM_OFF);
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

    int64_t inBudget   = settingsManager.getInt("usage.daily_in_budget",  1000000);
    int64_t outBudget  = settingsManager.getInt("usage.daily_out_budget", 100000);
    int64_t costBudget = settingsManager.getInt("usage.daily_cost_cents", 500);

    if (!snap.valid)
    {
        lv_label_set_text(labelStatus, "Waiting for /api/usage");
        lv_obj_set_style_text_color(labelStatus, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
        lv_bar_set_value(rowIn.bar,   0, LV_ANIM_OFF);
        lv_bar_set_value(rowOut.bar,  0, LV_ANIM_OFF);
        lv_bar_set_value(rowCost.bar, 0, LV_ANIM_OFF);
        lv_label_set_text(rowIn.amount,   "—");
        lv_label_set_text(rowOut.amount,  "—");
        lv_label_set_text(rowCost.amount, "—");
        lv_label_set_text(rowIn.detail,   "");
        lv_label_set_text(rowOut.detail,  "");
        lv_label_set_text(rowCost.detail, "");
        return;
    }

    // Status: how stale is the data
    TimeSpan age = now - snap.updatedAt;
    int ageSecs = (int)age.TotalSeconds();
    if (ageSecs < 0) ageSecs = 0;
    char statusBuf[48];
    if (ageSecs < 60)
        snprintf(statusBuf, sizeof(statusBuf), "Updated %ds ago", ageSecs);
    else if (ageSecs < 3600)
        snprintf(statusBuf, sizeof(statusBuf), "Updated %dm ago", ageSecs / 60);
    else
        snprintf(statusBuf, sizeof(statusBuf), "Updated %dh ago", ageSecs / 3600);
    lv_label_set_text(labelStatus, statusBuf);
    lv_obj_set_style_text_color(labelStatus,
        ageSecs < 300 ? lv_palette_main(LV_PALETTE_GREEN)
                      : lv_palette_main(LV_PALETTE_GREY),
        LV_PART_MAIN);

    // Input row: total input today (cache_read shown as secondary)
    {
        char usedStr[24], budgetStr[24], cacheStr[24];
        FormatTokens(usedStr,   sizeof(usedStr),   snap.inputTokensToday);
        FormatTokens(budgetStr, sizeof(budgetStr), inBudget);
        FormatTokens(cacheStr,  sizeof(cacheStr),  snap.cacheReadToday);

        char line[64];
        snprintf(line, sizeof(line), "%s / %s", usedStr, budgetStr);
        lv_label_set_text(rowIn.amount, line);
        snprintf(line, sizeof(line), "+%s cache", cacheStr);
        lv_label_set_text(rowIn.detail, line);

        SetBarPct(rowIn.bar, snap.inputTokensToday, inBudget);
    }

    // Output row
    {
        char usedStr[24], budgetStr[24];
        FormatTokens(usedStr,   sizeof(usedStr),   snap.outputTokensToday);
        FormatTokens(budgetStr, sizeof(budgetStr), outBudget);

        char line[64];
        snprintf(line, sizeof(line), "%s / %s", usedStr, budgetStr);
        lv_label_set_text(rowOut.amount, line);
        lv_label_set_text(rowOut.detail, "");

        SetBarPct(rowOut.bar, snap.outputTokensToday, outBudget);
    }

    // Cost row
    {
        char usedStr[24], budgetStr[24];
        FormatCost(usedStr,   sizeof(usedStr),   snap.costCentsToday);
        FormatCost(budgetStr, sizeof(budgetStr), costBudget);

        char line[64];
        snprintf(line, sizeof(line), "%s / %s", usedStr, budgetStr);
        lv_label_set_text(rowCost.amount, line);
        lv_label_set_text(rowCost.detail, "");

        SetBarPct(rowCost.bar, snap.costCentsToday, costBudget);
    }
}
