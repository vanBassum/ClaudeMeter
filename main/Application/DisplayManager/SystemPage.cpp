#include "SystemPage.h"
#include "SettingsManager/SettingsManager.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

void SystemPage::OnCreate()
{
    AddTopBar(LV_SYMBOL_SETTINGS " System");

    char buf[64] = {};

    settingsManager.getString("device.name", buf, sizeof(buf));
    AddTextRow("Name", buf, 50, 32);

    buf[0] = '\0';
    settingsManager.getString("ntp.server", buf, sizeof(buf));
    AddTextRow("NTP", buf, 90, 64);

    buf[0] = '\0';
    settingsManager.getString("ntp.timezone", buf, sizeof(buf));
    AddTextRow("Timezone", buf, 130, 64);

    lv_obj_t *saveBtn = AddButton(LV_SYMBOL_OK " Save & Reboot",
                                   lv_palette_main(LV_PALETTE_BLUE), 180, 40, SaveCb);
    lv_obj_align(saveBtn, LV_ALIGN_BOTTOM_RIGHT, -15, -15);
}

void SystemPage::SaveCb(lv_event_t *e)
{
    auto *self = static_cast<SystemPage *>(lv_event_get_user_data(e));

    static const char *keys[] = {"device.name", "ntp.server", "ntp.timezone"};
    int keyIdx = 0;
    uint32_t count = lv_obj_get_child_cnt(self->panel);
    for (uint32_t i = 0; i < count && keyIdx < 3; i++)
    {
        lv_obj_t *child = lv_obj_get_child(self->panel, i);
        if (lv_obj_check_type(child, &lv_textarea_class))
        {
            self->settingsManager.setString(keys[keyIdx], lv_textarea_get_text(child));
            keyIdx++;
        }
    }

    self->SaveAndReboot(self->settingsManager);
}
