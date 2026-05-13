#pragma once
#include "lvgl.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_ft5x06.h"
#include "esp_log.h"
#include "esp_timer.h"

class Display_WT32SC01
{
    inline static constexpr const char *TAG = "Display_WT32SC01";

    // --- LCD pins and parameters ---
    static constexpr gpio_num_t LCD_MOSI = GPIO_NUM_13;
    static constexpr gpio_num_t LCD_CLK  = GPIO_NUM_14;
    static constexpr gpio_num_t LCD_CS   = GPIO_NUM_15;
    static constexpr gpio_num_t LCD_DC   = GPIO_NUM_21;
    static constexpr gpio_num_t LCD_RST  = GPIO_NUM_22;
    static constexpr gpio_num_t LCD_BL   = GPIO_NUM_23;

    // --- Touch (FT6336) pins ---
    static constexpr gpio_num_t TOUCH_SDA = GPIO_NUM_18;
    static constexpr gpio_num_t TOUCH_SCL = GPIO_NUM_19;
    static constexpr gpio_num_t TOUCH_INT = GPIO_NUM_39;

public:
    static constexpr int LCD_HRES = 480;
    static constexpr int LCD_VRES = 320;

    Display_WT32SC01() = default;
    ~Display_WT32SC01();

    void Init();
    void SetBrightness(uint8_t percent);
    lv_disp_t* GetLvglDisplay() const { return disp; }

private:
    // --- Display ---
    esp_lcd_panel_handle_t panel = nullptr;
    lv_disp_draw_buf_t drawBuf;
    lv_color_t *buf1 = nullptr;
    lv_color_t *buf2 = nullptr;
    lv_disp_drv_t dispDrv;
    lv_disp_t *disp = nullptr;

    // --- Touch ---
    i2c_master_bus_handle_t i2cBus = nullptr;
    esp_lcd_touch_handle_t touch = nullptr;
    lv_indev_t *inputDev = nullptr;

    // --- Methods ---
    void InitBacklight();
    void InitTouch();
    static void LvglFlushCb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p);
    static void LvglTouchCb(lv_indev_drv_t *drv, lv_indev_data_t *data);
};
