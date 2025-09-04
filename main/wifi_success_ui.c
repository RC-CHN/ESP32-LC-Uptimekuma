#include "wifi_success_ui.h"
#include "wifi_manager.h"
#include "ui_events.h"
#include "esp_log.h"
#include <stdio.h>

static lv_obj_t * cont;
static lv_obj_t * ssid_label;
static lv_obj_t * ip_label;
static lv_obj_t * netmask_label;
static lv_obj_t * gw_label;

// Animation callback wrapper for LVGL v8
static void anim_y_cb(void * var, int32_t v)
{
    lv_obj_set_y(var, v);
}

// Forward declarations for event handlers
static void reconnect_event_cb(lv_event_t * e);
static void continue_event_cb(lv_event_t * e);

// Event handler for pressing diagnostics
static void debug_pressing_event_cb(lv_event_t * e)
{
    lv_obj_t * btn = lv_event_get_target(e);
    lv_indev_t * indev = lv_indev_get_act();
    if(indev == NULL)  return;

    lv_point_t point;
    lv_indev_get_point(indev, &point);

    ESP_LOGI("WIFI_SUCCESS_UI_DBG", "Button pos: x=%d, y=%d, w=%d, h=%d | Touch at: x=%d, y=%d",
             lv_obj_get_x(btn), lv_obj_get_y(btn),
             lv_obj_get_width(btn), lv_obj_get_height(btn),
             point.x, point.y);
}

void create_wifi_success_ui(lv_obj_t * parent)
{
    cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_obj_get_width(parent), lv_obj_get_height(parent) - 20); // Below status bar
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN); // Initially hidden
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);


    lv_obj_t * title = lv_label_create(cont);
    lv_label_set_text(title, "Connection Successful!");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    ssid_label = lv_label_create(cont);
    lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, 10, 40);
    lv_label_set_text(ssid_label, "SSID: ");

    ip_label = lv_label_create(cont);
    lv_obj_align(ip_label, LV_ALIGN_TOP_LEFT, 10, 65);
    lv_label_set_text(ip_label, "IP: ");

    netmask_label = lv_label_create(cont);
    lv_obj_align(netmask_label, LV_ALIGN_TOP_LEFT, 10, 90);
    lv_label_set_text(netmask_label, "Netmask: ");

    gw_label = lv_label_create(cont);
    lv_obj_align(gw_label, LV_ALIGN_TOP_LEFT, 10, 115);
    lv_label_set_text(gw_label, "Gateway: ");

    // Reconnect Button
    lv_obj_t * reconnect_btn = lv_btn_create(cont);
    lv_obj_align(reconnect_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_event_cb(reconnect_btn, reconnect_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * reconnect_label = lv_label_create(reconnect_btn);
    lv_label_set_text(reconnect_label, "Reconnect");
    lv_obj_center(reconnect_label);

    // Continue Button
    lv_obj_t * continue_btn = lv_btn_create(cont);
    lv_obj_align(continue_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_add_event_cb(continue_btn, continue_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(continue_btn, debug_pressing_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_t * continue_label = lv_label_create(continue_btn);
    lv_label_set_text(continue_label, "Continue");
    lv_obj_center(continue_label);
}

void wifi_success_ui_update_info(void)
{
    char buf[64];
    esp_netif_ip_info_t ip_info;

    wifi_manager_get_ssid(buf, sizeof(buf));
    lv_label_set_text_fmt(ssid_label, "SSID: %s", buf);

    if (wifi_manager_get_ip_info(&ip_info) == ESP_OK) {
        sprintf(buf, "IP: " IPSTR, IP2STR(&ip_info.ip));
        lv_label_set_text(ip_label, buf);

        sprintf(buf, "Netmask: " IPSTR, IP2STR(&ip_info.netmask));
        lv_label_set_text(netmask_label, buf);

        sprintf(buf, "Gateway: " IPSTR, IP2STR(&ip_info.gw));
        lv_label_set_text(gw_label, buf);
    }
}


void wifi_success_ui_show(bool show) {
    if (cont == NULL) return;

    if (show) {
        wifi_success_ui_update_info();
        lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN);
        
        // Animation from top
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, cont);
        lv_anim_set_values(&a, -lv_obj_get_height(cont), 0);
        lv_anim_set_time(&a, 500);
        lv_anim_set_exec_cb(&a, anim_y_cb);
        lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
        lv_anim_start(&a);

    } else {
        lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
    }
}

static void reconnect_event_cb(lv_event_t * e)
{
    ESP_LOGI("WIFI_SUCCESS_UI", "Reconnect button clicked");
    wifi_manager_clear_config();
    // Send event to main app to handle UI switching
    lv_event_send(lv_scr_act(), EVT_SWITCH_TO_CONFIG_UI, NULL);
}

static void continue_event_cb(lv_event_t * e)
{
    ESP_LOGI("WIFI_SUCCESS_UI", "Continue button clicked");
    // Send event to main app to handle UI switching
    lv_event_send(lv_scr_act(), EVT_SWITCH_TO_KUMA_UI, NULL);
}
