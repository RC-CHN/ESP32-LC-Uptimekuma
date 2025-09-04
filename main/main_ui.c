#include "main_ui.h"
#include "uptime_kuma_ui.h"
#include "esp_log.h"
#include "app_events.h"

static const char *TAG = "MAIN_UI";
static lv_obj_t *main_container = NULL;

static void reconfigure_wifi_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Reconfigure WiFi button clicked");
        app_show_wifi_config_screen();
    }
}

void create_main_ui(lv_obj_t *parent) {
    if (main_container) {
        lv_obj_clean(main_container);
    } else {
        main_container = lv_obj_create(parent);
        lv_obj_remove_style_all(main_container);
        lv_obj_set_size(main_container, lv_obj_get_width(parent), lv_obj_get_height(parent) - 20); // Make space for status bar
        lv_obj_align(main_container, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_pad_all(main_container, 0, 0);
        lv_obj_set_style_border_width(main_container, 0, 0);
        lv_obj_clear_flag(main_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(main_container, LV_OBJ_FLAG_HIDDEN); // Initially hidden
    }

    // Create the TabView
    lv_obj_t *tabview = lv_tabview_create(main_container, LV_DIR_TOP, 40);
    lv_obj_set_size(tabview, LV_PCT(100), LV_PCT(100));
    lv_obj_align(tabview, LV_ALIGN_CENTER, 0, 0);


    // Add Uptime Kuma Tab
    lv_obj_t *kuma_tab = lv_tabview_add_tab(tabview, "Uptime");
    create_uptime_kuma_ui(kuma_tab);

    // Add a placeholder Settings tab
    lv_obj_t *settings_tab = lv_tabview_add_tab(tabview, "Settings");
    
    // Add a button to reconfigure WiFi
    lv_obj_t * btn = lv_btn_create(settings_tab);
    lv_obj_add_event_cb(btn, reconfigure_wifi_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "Reconfigure WiFi");
    lv_obj_center(label);
    
    ESP_LOGI(TAG, "Main UI with TabView created.");
}

void main_ui_show(bool show) {
    if (main_container == NULL) return;
    if (show) {
        lv_obj_clear_flag(main_container, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(main_container, LV_OBJ_FLAG_HIDDEN);
    }
}