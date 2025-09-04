#include <string.h>
#include "wifi_config_ui.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "wifi_manager.h"

static lv_obj_t * kb;
static lv_obj_t * ta_ssid;
static lv_obj_t * ta_pwd;
static lv_obj_t * cont;

static void ta_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);
    if(code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(kb, ta);
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }

    if(code == LV_EVENT_DEFOCUSED) {
        lv_keyboard_set_textarea(kb, NULL);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}

static void connect_event_cb(lv_event_t * e)
{
    const char * ssid = lv_textarea_get_text(ta_ssid);
    const char * pwd = lv_textarea_get_text(ta_pwd);

    wifi_manager_connect(ssid, pwd);
}

void create_wifi_config_ui(lv_disp_t *disp)
{
    lv_obj_t *scr = lv_disp_get_scr_act(disp);

    // Create a container for the wifi config UI, below the status bar
    cont = lv_obj_create(scr);
    lv_obj_set_size(cont, lv_obj_get_width(scr), lv_obj_get_height(scr) - 20); // Adjust size to not overlap status bar
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);


    // Create a keyboard
    kb = lv_keyboard_create(scr); // Keyboard should be on the screen to cover everything
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    // Create SSID TextArea
    ta_ssid = lv_textarea_create(cont);
    lv_obj_set_width(ta_ssid, lv_pct(80));
    lv_textarea_set_one_line(ta_ssid, true);
    lv_obj_align(ta_ssid, LV_ALIGN_TOP_MID, 0, 20);
    lv_textarea_set_placeholder_text(ta_ssid, "SSID");

    // Create Password TextArea
    ta_pwd = lv_textarea_create(cont);
    lv_obj_set_width(ta_pwd, lv_pct(80));
    lv_textarea_set_one_line(ta_pwd, true);
    lv_textarea_set_password_mode(ta_pwd, true);
    lv_obj_align(ta_pwd, LV_ALIGN_TOP_MID, 0, 70);
    lv_textarea_set_placeholder_text(ta_pwd, "Password");


    lv_obj_add_event_cb(ta_ssid, ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ta_pwd, ta_event_cb, LV_EVENT_ALL, NULL);

    // Create a connect button
    lv_obj_t * connect_btn = lv_btn_create(cont);
    lv_obj_align(connect_btn, LV_ALIGN_TOP_MID, 0, 120);
    lv_obj_t * label = lv_label_create(connect_btn);
    lv_label_set_text(label, "Connect");
    lv_obj_center(label);
    lv_obj_add_event_cb(connect_btn, connect_event_cb, LV_EVENT_CLICKED, NULL);
}

void wifi_config_ui_show(bool show)
{
    if (cont == NULL) return;
    if (show) {
        vTaskDelay(pdMS_TO_TICKS(50));
        lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN);
        lv_obj_invalidate(lv_scr_act());
    } else {
        lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
    }
}