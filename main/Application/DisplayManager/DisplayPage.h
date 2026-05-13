#pragma once
#include "lvgl.h"
#include "esp_log.h"
#include <functional>

class ServiceProvider;
class SettingsManager;

using NavigateFunc = std::function<void(const char *)>;

class DisplayPage
{
public:
    static constexpr int LCD_HRES = 480;
    static constexpr int LCD_VRES = 320;

    virtual ~DisplayPage() = default;

    void Show(lv_obj_t *parent)
    {
        if (panel)
            return;
        panel = CreatePanel(parent);
        OnCreate();
    }

    void Hide()
    {
        if (panel)
        {
            keyboard = nullptr;
            lv_obj_del(panel);
            panel = nullptr;
        }
    }

    bool IsVisible() const { return panel != nullptr; }

    virtual void Update() {}

    void SetNavigator(NavigateFunc nav) { navigate = nav; }

protected:
    lv_obj_t *panel = nullptr;
    lv_obj_t *keyboard = nullptr;
    NavigateFunc navigate;

    virtual void OnCreate() = 0;

    // ── UI Helpers ───────────────────────────────────────────

    static lv_obj_t *CreatePanel(lv_obj_t *parent)
    {
        lv_obj_t *p = lv_obj_create(parent);
        lv_obj_set_size(p, LCD_HRES, LCD_VRES);
        lv_obj_set_pos(p, 0, 0);
        lv_obj_set_style_bg_color(p, lv_color_hex(0x101010), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(p, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(p, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(p, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(p, 0, LV_PART_MAIN);
        lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
        return p;
    }

    lv_obj_t *AddTopBar(const char *title, bool showBack = true)
    {
        if (showBack)
        {
            lv_obj_t *btn = lv_btn_create(panel);
            lv_obj_set_size(btn, 70, 32);
            lv_obj_set_pos(btn, 8, 6);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x303030), LV_PART_MAIN);
            lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);

            lv_obj_t *lbl = lv_label_create(btn);
            lv_label_set_text(lbl, LV_SYMBOL_LEFT " Back");
            lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
            lv_obj_center(lbl);
            lv_obj_add_event_cb(btn, [](lv_event_t *e) {
                auto *page = static_cast<DisplayPage *>(lv_event_get_user_data(e));
                if (page->navigate)
                    page->navigate("back");
            }, LV_EVENT_CLICKED, this);
        }

        lv_obj_t *lbl = lv_label_create(panel);
        lv_label_set_text(lbl, title);
        lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 10);
        return lbl;
    }

    lv_obj_t *AddButton(const char *text, lv_color_t color,
                        lv_coord_t w, lv_coord_t h, lv_event_cb_t cb, void *userData = nullptr)
    {
        lv_obj_t *btn = lv_btn_create(panel);
        lv_obj_set_size(btn, w, h);
        lv_obj_set_style_bg_color(btn, color, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, userData ? userData : this);
        return btn;
    }

    lv_obj_t *AddTextRow(const char *label, const char *value, lv_coord_t y,
                         int maxLen, bool password = false)
    {
        lv_obj_t *lbl = lv_label_create(panel);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_pos(lbl, 12, y + 8);

        lv_obj_t *ta = lv_textarea_create(panel);
        lv_obj_set_size(ta, 280, 34);
        lv_obj_set_pos(ta, 130, y);
        lv_textarea_set_one_line(ta, true);
        lv_textarea_set_max_length(ta, maxLen);
        lv_textarea_set_text(ta, value);
        if (password)
            lv_textarea_set_password_mode(ta, true);

        lv_obj_set_style_bg_color(ta, lv_color_hex(0x252525), LV_PART_MAIN);
        lv_obj_set_style_text_color(ta, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_border_color(ta, lv_color_hex(0x444444), LV_PART_MAIN);

        lv_obj_add_event_cb(ta, [](lv_event_t *e) {
            auto *page = static_cast<DisplayPage *>(lv_event_get_user_data(e));
            page->ShowKeyboard(lv_event_get_target(e));
        }, LV_EVENT_FOCUSED, this);

        // Hide keyboard when textarea accepts (OK) or cancels
        lv_obj_add_event_cb(ta, [](lv_event_t *e) {
            auto *page = static_cast<DisplayPage *>(lv_event_get_user_data(e));
            page->HideKeyboard();
        }, LV_EVENT_READY, this);

        lv_obj_add_event_cb(ta, [](lv_event_t *e) {
            auto *page = static_cast<DisplayPage *>(lv_event_get_user_data(e));
            page->HideKeyboard();
        }, LV_EVENT_CANCEL, this);

        return ta;
    }

    void HideKeyboard()
    {
        if (keyboard)
            lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }

    void ShowKeyboard(lv_obj_t *textarea)
    {
        if (!keyboard)
        {
            keyboard = lv_keyboard_create(panel);
            lv_obj_set_size(keyboard, LCD_HRES, 130);
            lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
            lv_obj_set_style_text_color(keyboard, lv_color_white(), LV_PART_ITEMS);
            lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x333333), LV_PART_ITEMS);

        }
        lv_keyboard_set_textarea(keyboard, textarea);
        lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }

    void SaveAndReboot(SettingsManager &settings);
};
