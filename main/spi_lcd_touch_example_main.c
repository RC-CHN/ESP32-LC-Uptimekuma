/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "myi2c.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "wifi_manager.h"

#if CONFIG_EXAMPLE_LCD_CONTROLLER_ILI9341
#include "esp_lcd_ili9341.h"
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_GC9A01
#include "esp_lcd_gc9a01.h"
#endif

#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_STMPE610
#include "esp_lcd_touch_stmpe610.h"
#elif CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_FT6336
#include "esp_lcd_touch_ft5x06.h"
#endif

#include "wifi_config_ui.h"
#include "wifi_success_ui.h"
#include "gxhtc3.h"
#include "ui_events.h"
#include "uptime_kuma.h"
#include "uptime_kuma_ui.h"
#include "landing_page.h"

static const char *TAG = "example";

// --- Uptime Kuma Task ---
static TaskHandle_t kuma_task_handle = NULL;

static void update_kuma_ui_cb(void *data) {
    kuma_monitor_list_t *monitor_list = (kuma_monitor_list_t *)data;
    update_uptime_kuma_ui(monitor_list);
    // The UI now owns the data, but we must free it after the update
    kuma_client_free_monitor_list(monitor_list);
}

static void kuma_client_task(void *pvParameters) {
    ESP_LOGI(TAG, "Kuma Client Task started.");
    while (1) {
        kuma_monitor_list_t *monitor_list = NULL;
        esp_err_t err = kuma_client_fetch_data("main", &monitor_list);

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Successfully fetched Kuma data. %d monitors.", monitor_list ? monitor_list->count : 0);
            // Safely call LVGL function from another thread
            lv_async_call(update_kuma_ui_cb, monitor_list);
        } else {
            ESP_LOGE(TAG, "Failed to fetch Kuma data.");
            // If list was partially allocated, free it
            if (monitor_list) {
                kuma_client_free_monitor_list(monitor_list);
            }
        }
        
        ESP_LOGI(TAG, "Kuma task sleeping for 10 seconds...");
        vTaskDelay(pdMS_TO_TICKS(10000)); // 10-second TTL
    }
}
// --- End Uptime Kuma Task ---

static lv_obj_t *wifi_status_label;
static lv_obj_t *temp_label;
static lv_obj_t *humi_label;

static void _user_interaction_cb(lv_event_t * e)
{
    uint32_t *last_user_interaction = (uint32_t *)lv_event_get_user_data(e);
    *last_user_interaction = lv_tick_get();
}

static void ui_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == EVT_SWITCH_TO_CONFIG_UI) {
        wifi_success_ui_show(false);
        wifi_config_ui_show(true);
    } else if (code == EVT_SWITCH_TO_MAIN_APP) {
        wifi_success_ui_show(false);
        // Here you would show your main application screen
    } else if (code == EVT_SWITCH_TO_KUMA_UI) {
        ESP_LOGI(TAG, "Switching to Uptime Kuma UI");
        wifi_success_ui_show(false);
        uptime_kuma_ui_show(true);

        // Start the Kuma client task if it's not already running
        if (kuma_task_handle == NULL) {
            xTaskCreate(kuma_client_task, "kuma_client_task", 8192, NULL, 5, &kuma_task_handle);
        }
    }
}


static void wifi_status_update_cb(wifi_manager_status_t status)
{
    if (wifi_status_label) {
        switch (status) {
            case WIFI_STATUS_DISCONNECTED:
                lv_label_set_text(wifi_status_label, "#ff0000 " LV_SYMBOL_CLOSE);
                wifi_success_ui_show(false);
                wifi_config_ui_show(true);
                break;
            case WIFI_STATUS_CONNECTING:
                lv_label_set_text(wifi_status_label, "#ffff00 " LV_SYMBOL_REFRESH);
                break;
            case WIFI_STATUS_CONNECTED:
                lv_label_set_text(wifi_status_label, "#00ff00 " LV_SYMBOL_WIFI);
                wifi_config_ui_show(false);
                wifi_success_ui_show(true);
                break;
        }
    }
}

// Using SPI2 in the example
#define LCD_HOST  SPI2_HOST

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (40 * 1000 * 1000)
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  0
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_SCLK           3
#define EXAMPLE_PIN_NUM_MOSI           5
#define EXAMPLE_PIN_NUM_MISO           -1
#define EXAMPLE_PIN_NUM_LCD_DC         6
#define EXAMPLE_PIN_NUM_LCD_RST        -1
#define EXAMPLE_PIN_NUM_LCD_CS         4
#define EXAMPLE_PIN_NUM_BK_LIGHT       2
#define EXAMPLE_PIN_NUM_TOUCH_CS       -1

// The pixel number in horizontal and vertical
#if CONFIG_EXAMPLE_LCD_CONTROLLER_ILI9341
#define EXAMPLE_LCD_H_RES              240
#define EXAMPLE_LCD_V_RES              320
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_GC9A01
#define EXAMPLE_LCD_H_RES              240
#define EXAMPLE_LCD_V_RES              240
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_ST7789
#define EXAMPLE_LCD_H_RES              240
#define EXAMPLE_LCD_V_RES              320
#endif
// Bit number used to represent command and parameter
#define EXAMPLE_LCD_CMD_BITS           8
#define EXAMPLE_LCD_PARAM_BITS         8

#define EXAMPLE_LVGL_TICK_PERIOD_MS    2


#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
esp_lcd_touch_handle_t tp = NULL;
#endif

// extern void example_lvgl_demo_ui(lv_disp_t *disp);

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

/* Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated. */
static void example_lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, false);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        // Rotate LCD touch
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_90:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, true);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        // Rotate LCD touch
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_180:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, true);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        // Rotate LCD touch
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    case LV_DISP_ROT_270:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, false);
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
        // Rotate LCD touch
        esp_lcd_touch_set_mirror_y(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
#endif
        break;
    }
}

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
static void example_lvgl_touch_cb(lv_indev_drv_t * drv, lv_indev_data_t * data)
{
    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;

    /* Read touch controller data */
    esp_lcd_touch_read_data(drv->user_data);

    /* Get coordinates */
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(drv->user_data, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);

    if (touchpad_pressed && touchpad_cnt > 0) {
        data->point.x = touchpad_x[0];
        data->point.y = touchpad_y[0];
        data->state = LV_INDEV_STATE_PRESSED;
        //ESP_LOGI(TAG, "Touch: PRESSED at x=%d, y=%d", data->point.x, data->point.y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
#endif

static void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    // Initialize GXHTC3 Sensor
    if (gxhtc3_read_id() == ESP_OK) {
        ESP_LOGI(TAG, "GXHTC3 sensor found.");
    } else {
        ESP_LOGE(TAG, "GXHTC3 sensor not found.");
    }


    wifi_manager_init();
    // wifi_manager_load_sta_config_and_connect(); // We will do this after a delay in the main loop

    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv;      // contains callback functions

    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .sclk_io_num = EXAMPLE_PIN_NUM_SCLK,
        .mosi_io_num = EXAMPLE_PIN_NUM_MOSI,
        .miso_io_num = EXAMPLE_PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = EXAMPLE_PIN_NUM_LCD_DC,
        .cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = example_notify_lvgl_flush_ready,
        .user_ctx = &disp_drv,
    };
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
#if CONFIG_EXAMPLE_LCD_CONTROLLER_ILI9341
    ESP_LOGI(TAG, "Install ILI9341 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_GC9A01
    ESP_LOGI(TAG, "Install GC9A01 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_ST7789
    ESP_LOGI(TAG, "Install ST7789 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
#endif

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
#if CONFIG_EXAMPLE_LCD_CONTROLLER_GC9A01
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
#endif
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    // user can flush pre-defined pattern to the screen before we turn on the screen or backlight
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
   // Attach the TOUCH to the I2C bus
   ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_MASTER_NUM, &tp_io_config, &tp_io_handle));

   esp_lcd_touch_config_t tp_cfg = {
       .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_STMPE610
    ESP_LOGI(TAG, "Initialize touch controller STMPE610");
    ESP_ERROR_CHECK(esp_lcd_touch_new_spi_stmpe610(tp_io_handle, &tp_cfg, &tp));
#endif // CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_STMPE610
#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_FT6336
    ESP_LOGI(TAG, "Initialize touch controller FT6336");
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp));
#endif // CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_FT6336
#endif // CONFIG_EXAMPLE_LCD_TOUCH_ENABLED

    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    lv_color_t *buf1 = heap_caps_malloc(EXAMPLE_LCD_H_RES * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = heap_caps_malloc(EXAMPLE_LCD_H_RES * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2);
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * 20);

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = example_lvgl_flush_cb;
    disp_drv.drv_update_cb = example_lvgl_port_update_callback;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
    static lv_indev_drv_t indev_drv;    // Input device driver (Touch)
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = disp;
    indev_drv.read_cb = example_lvgl_touch_cb;
    indev_drv.user_data = tp;

    lv_indev_drv_register(&indev_drv);
#endif

    ESP_LOGI(TAG, "Display WiFi Config UI");

    // Create a status bar
    lv_obj_t *status_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(status_bar, EXAMPLE_LCD_H_RES, 20);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 0, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);

    temp_label = lv_label_create(status_bar);
    lv_obj_align(temp_label, LV_ALIGN_LEFT_MID, 5, 0);
    lv_label_set_text(temp_label, "T: --");

    humi_label = lv_label_create(status_bar);
    lv_obj_align(humi_label, LV_ALIGN_LEFT_MID, 70, 0);
    lv_label_set_text(humi_label, "H: --");

    wifi_status_label = lv_label_create(status_bar);
    lv_label_set_recolor(wifi_status_label, true);
    lv_obj_align(wifi_status_label, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_label_set_text(wifi_status_label, ""); // Initially empty

    wifi_manager_set_status_callback(wifi_status_update_cb);
    
    lv_obj_add_event_cb(lv_scr_act(), ui_event_handler, LV_EVENT_ALL, NULL);

    create_wifi_config_ui(disp);
    create_wifi_success_ui(lv_scr_act());
    create_uptime_kuma_ui(lv_scr_act());
    create_landing_page_ui(lv_scr_act());

    // Initially, show only the landing page
    wifi_config_ui_show(false);
    wifi_success_ui_show(false);
    uptime_kuma_ui_show(false);
    landing_page_ui_show(true);
    
    // Let the landing page show for a bit, while running the LVGL handler to play animations
    ESP_LOGI(TAG, "Showing landing page for 1.5 seconds...");
    uint32_t landing_start_time = lv_tick_get();
    while (lv_tick_get() - landing_start_time < 1500) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    landing_page_ui_show(false);
    ESP_LOGI(TAG, "Landing page hidden.");


    bool auto_connect_attempted = false;
    uint32_t last_sensor_update = 0;
    uint32_t last_user_interaction = 0;
    uint32_t last_carousel_update = 0;
    bool carousel_active = false;

    // Register a callback to detect user interaction
    lv_obj_add_event_cb(lv_scr_act(),_user_interaction_cb, LV_EVENT_PRESSED, &last_user_interaction);


    while (1) {
        // --- Auto-connect logic ---
        if (!auto_connect_attempted) {
            ESP_LOGI(TAG, "Attempting to connect with stored credentials...");
            if (!wifi_manager_load_sta_config_and_connect()) {
                wifi_status_update_cb(WIFI_STATUS_DISCONNECTED);
            }
            auto_connect_attempted = true;
            last_user_interaction = lv_tick_get(); // Start the timer after setup
        }

        // --- Sensor update logic ---
        if (lv_tick_get() - last_sensor_update > 2000) {
            if (gxhtc3_get_tah() == ESP_OK) {
                char temp_str[10];
                char humi_str[10];
                sprintf(temp_str, "T: %dC", temp_int);
                sprintf(humi_str, "H: %d%%", humi_int);
                lv_label_set_text(temp_label, temp_str);
                lv_label_set_text(humi_label, humi_str);
            }
            last_sensor_update = lv_tick_get();
        }

        // --- Carousel logic ---
        uint32_t now = lv_tick_get();
        if (!carousel_active && (now - last_user_interaction > 30000)) {
            ESP_LOGI(TAG, "User idle for 30s, starting carousel.");
            carousel_active = true;
            last_carousel_update = now;
            uptime_kuma_ui_carousel_next(); // Switch to the first one immediately
        }

        if (carousel_active) {
            if (now - last_carousel_update > 10000) {
                 ESP_LOGI(TAG, "Carousel updating to next slide.");
                 uptime_kuma_ui_carousel_next();
                 last_carousel_update = now;
            }
        }
        
        if (carousel_active && lv_indev_get_act() && lv_indev_get_gesture_dir(lv_indev_get_act()) != LV_DIR_NONE) {
            ESP_LOGI(TAG, "User interaction detected, stopping carousel.");
            carousel_active = false;
            last_user_interaction = now;
        }


        // --- LVGL Handler ---
        vTaskDelay(pdMS_TO_TICKS(5));
        lv_timer_handler();
    }
}

