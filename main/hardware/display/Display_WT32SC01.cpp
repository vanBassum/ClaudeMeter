#include "Display_WT32SC01.h"
#include "esp_lcd_st7796.h"
#include "esp_lcd_touch_ft5x06.h"
#include <cassert>

Display_WT32SC01::~Display_WT32SC01()
{
    if (panel)
        esp_lcd_panel_del(panel);
    if (touch)
        esp_lcd_touch_del(touch);
    free(buf1);
    free(buf2);
}

void Display_WT32SC01::Init()
{
    ESP_LOGI(TAG, "Initializing WT32-SC01 display");

    // --- SPI bus for LCD ---
    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num = LCD_CLK;
    buscfg.mosi_io_num = LCD_MOSI;
    buscfg.miso_io_num = -1;
    buscfg.max_transfer_sz = LCD_HRES * 40 * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // --- SPI IO config ---
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.dc_gpio_num = LCD_DC;
    io_cfg.cs_gpio_num = LCD_CS;
    io_cfg.pclk_hz = 40 * 1000 * 1000;
    io_cfg.lcd_cmd_bits = 8;
    io_cfg.lcd_param_bits = 8;
    io_cfg.trans_queue_depth = 10;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_cfg, &io));

    // --- ST7796 panel config ---
    esp_lcd_panel_dev_config_t panel_cfg = {};
    panel_cfg.reset_gpio_num = LCD_RST;
    panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    panel_cfg.bits_per_pixel = 16;

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7796(io, &panel_cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    // --- Backlight ---
    InitBacklight();

    // --- LVGL display setup ---
    size_t buf_pixels = LCD_HRES * 32;
    buf1 = (lv_color_t *)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
    buf2 = (lv_color_t *)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 && buf2);

    lv_disp_draw_buf_init(&drawBuf, buf1, buf2, buf_pixels);

    lv_disp_drv_init(&dispDrv);
    dispDrv.flush_cb = LvglFlushCb;
    dispDrv.draw_buf = &drawBuf;
    dispDrv.hor_res = LCD_HRES;
    dispDrv.ver_res = LCD_VRES;
    dispDrv.user_data = this;
    disp = lv_disp_drv_register(&dispDrv);

    ESP_LOGI(TAG, "WT32-SC01 LVGL display registered");

    // --- Touch ---
    InitTouch();
}

void Display_WT32SC01::InitTouch()
{
    ESP_LOGI(TAG, "Initializing FT6336 touch controller");

    // --- I2C master bus ---
    i2c_master_bus_config_t i2c_bus_cfg = {};
    i2c_bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_bus_cfg.i2c_port = I2C_NUM_0;
    i2c_bus_cfg.sda_io_num = TOUCH_SDA;
    i2c_bus_cfg.scl_io_num = TOUCH_SCL;
    i2c_bus_cfg.glitch_ignore_cnt = 7;
    i2c_bus_cfg.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2cBus));

    // --- Touch panel IO ---
    esp_lcd_panel_io_handle_t tp_io = nullptr;
    esp_lcd_panel_io_i2c_config_t io_cfg = {};
    io_cfg.dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS;
    io_cfg.scl_speed_hz = 400000;
    io_cfg.control_phase_bytes = 1;
    io_cfg.lcd_cmd_bits = 8;
    io_cfg.lcd_param_bits = 0;
    io_cfg.flags.dc_low_on_data = 0;
    io_cfg.flags.disable_control_phase = 1;

    esp_err_t err = esp_lcd_new_panel_io_i2c(i2cBus, &io_cfg, &tp_io);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to create touch panel IO: %s", esp_err_to_name(err));
        return;
    }

    // --- Touch configuration ---
    esp_lcd_touch_config_t tp_cfg = {};
    tp_cfg.x_max = LCD_VRES;
    tp_cfg.y_max = LCD_HRES;
    tp_cfg.rst_gpio_num = GPIO_NUM_NC;
    tp_cfg.int_gpio_num = TOUCH_INT;
    tp_cfg.levels.interrupt = 0;
    tp_cfg.flags.swap_xy = true;
    tp_cfg.flags.mirror_x = true;
    tp_cfg.flags.mirror_y = false;

    err = esp_lcd_touch_new_i2c_ft5x06(tp_io, &tp_cfg, &touch);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "FT6336 init failed: %s — touch disabled", esp_err_to_name(err));
        touch = nullptr;
        return;
    }

    // --- Register input device with LVGL ---
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = LvglTouchCb;
    indev_drv.user_data = this;
    inputDev = lv_indev_drv_register(&indev_drv);

    ESP_LOGI(TAG, "FT6336 touch initialized successfully");
}


void Display_WT32SC01::LvglTouchCb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    auto *self = static_cast<Display_WT32SC01 *>(drv->user_data);
    if (!self || !self->touch)
        return;

    esp_lcd_touch_read_data(self->touch);

    uint16_t x, y;
    uint8_t points = 1;
    if (esp_lcd_touch_get_coordinates(self->touch, &x, &y, NULL, &points, 1) && points > 0) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
        ESP_LOGD(TAG, "Touch: x=%d y=%d", x, y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}


void Display_WT32SC01::LvglFlushCb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    auto *self = static_cast<Display_WT32SC01 *>(drv->user_data);
    if (!self || !self->panel) {
        lv_disp_flush_ready(drv);
        return;
    }

    esp_err_t err = esp_lcd_panel_draw_bitmap(
        self->panel,
        area->x1, area->y1,
        area->x2 + 1, area->y2 + 1,
        color_p);

    if (err != ESP_OK)
        ESP_LOGE(TAG, "Flush failed: %s", esp_err_to_name(err));

    lv_disp_flush_ready(drv);
}


void Display_WT32SC01::InitBacklight()
{
    ledc_timer_config_t tcfg = {};
    tcfg.speed_mode = LEDC_LOW_SPEED_MODE;
    tcfg.timer_num = LEDC_TIMER_0;
    tcfg.duty_resolution = LEDC_TIMER_10_BIT;
    tcfg.freq_hz = 5000;
    tcfg.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&tcfg));

    ledc_channel_config_t ccfg = {};
    ccfg.gpio_num = LCD_BL;
    ccfg.speed_mode = LEDC_LOW_SPEED_MODE;
    ccfg.channel = LEDC_CHANNEL_0;
    ccfg.timer_sel = LEDC_TIMER_0;
    ccfg.duty = 512;
    ccfg.hpoint = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&ccfg));
}

void Display_WT32SC01::SetBrightness(uint8_t percent)
{
    if (percent > 100)
        percent = 100;
    uint32_t duty = (1 << 10) * percent / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}
