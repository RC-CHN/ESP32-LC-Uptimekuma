#include "uptime_kuma_ui.h"
#include "esp_log.h"

static const char *TAG = "KUMA_UI";

static lv_obj_t *kuma_container = NULL;

// 内部函数，用于为单个监控项创建卡片UI
static void create_monitor_card(lv_obj_t *parent, const kuma_monitor_t *monitor) {
    // 卡片容器
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, LV_PCT(90), LV_PCT(80));
    lv_obj_center(card);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 监控项名称
    lv_obj_t *name_label = lv_label_create(card);
    lv_label_set_text(name_label, monitor->name);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);

    // 状态行容器
    lv_obj_t *status_cont = lv_obj_create(card);
    lv_obj_remove_style_all(status_cont); // 移除背景等样式
    lv_obj_set_width(status_cont, LV_PCT(100));
    lv_obj_set_flex_flow(status_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(status_cont, 10, 0);

    // 状态指示灯
    lv_obj_t *status_led = lv_led_create(status_cont);
    lv_obj_set_size(status_led, 20, 20);
    switch (monitor->status) {
        case KUMA_MONITOR_STATUS_UP:
            lv_led_set_color(status_led, lv_palette_main(LV_PALETTE_GREEN));
            break;
        case KUMA_MONITOR_STATUS_DOWN:
            lv_led_set_color(status_led, lv_palette_main(LV_PALETTE_RED));
            break;
        default:
            lv_led_set_color(status_led, lv_palette_main(LV_PALETTE_GREY));
            break;
    }
    lv_led_on(status_led);

    // 状态文本
    lv_obj_t *status_label = lv_label_create(status_cont);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    switch (monitor->status) {
        case KUMA_MONITOR_STATUS_UP:
            lv_label_set_text(status_label, "UP");
            break;
        case KUMA_MONITOR_STATUS_DOWN:
            lv_label_set_text(status_label, "DOWN");
            break;
        default:
            lv_label_set_text(status_label, "Unknown");
            break;
    }

    lv_obj_t *ping_label = lv_label_create(card);
    lv_label_set_text_fmt(ping_label, "Ping: %d ms", monitor->ping);
    lv_obj_set_style_text_font(ping_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ping_label, lv_color_hex(0x8a8a8a), 0);
}

void create_uptime_kuma_ui(lv_obj_t *parent) {
    if (kuma_container) {
        lv_obj_clean(kuma_container);
    } else {
        kuma_container = lv_obj_create(parent);
        lv_obj_remove_style_all(kuma_container);
        lv_obj_set_size(kuma_container, LV_PCT(100), LV_PCT(100));
        lv_obj_clear_flag(kuma_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(kuma_container, LV_OBJ_FLAG_CLICKABLE); 
    }
    ESP_LOGI(TAG, "Uptime Kuma UI container created.");
}

void update_uptime_kuma_ui(const kuma_monitor_list_t *monitor_list) {
    if (!kuma_container) {
        ESP_LOGE(TAG, "UI container not created. Call create_uptime_kuma_ui first.");
        return;
    }
    
    lv_obj_clean(kuma_container);

    if (!monitor_list || monitor_list->count == 0) {
        lv_obj_t *label = lv_label_create(kuma_container);
        lv_label_set_text(label, "No monitors found.");
        lv_obj_center(label);
        return;
    }
    
    lv_obj_t *tileview = lv_tileview_create(kuma_container);
    lv_obj_set_size(tileview, LV_PCT(100), LV_PCT(100));
    lv_obj_align(tileview, LV_ALIGN_TOP_LEFT, 0, 0);

    for (int i = 0; i < monitor_list->count; i++) {
        lv_obj_t *tile = lv_tileview_add_tile(tileview, i, 0, LV_DIR_HOR);
        if (tile) {
            create_monitor_card(tile, monitor_list->monitors[i]);
        }
    }
    ESP_LOGI(TAG, "Updated Kuma UI with %d monitors.", monitor_list->count);
}
