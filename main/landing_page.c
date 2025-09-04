#include "landing_page.h"

static lv_obj_t *landing_container = NULL;

void create_landing_page_ui(lv_obj_t *parent)
{
    if (landing_container != NULL) {
        return; // Already created
    }

    // Create a container that fills the screen
    landing_container = lv_obj_create(parent);
    lv_obj_remove_style_all(landing_container);
    lv_obj_set_size(landing_container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(landing_container);
    lv_obj_set_style_bg_color(landing_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(landing_container, LV_OPA_COVER, 0);


    // Create a spinner
    lv_obj_t *spinner = lv_spinner_create(landing_container, 1000, 60);
    lv_obj_set_size(spinner, 100, 100);
    lv_obj_center(spinner);

    // Create a label below the spinner
    lv_obj_t *label = lv_label_create(landing_container);
    lv_label_set_text(label, "Starting...");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align_to(label, spinner, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
}

void landing_page_ui_show(bool show)
{
    if (landing_container == NULL) return;
    if (show) {
        lv_obj_clear_flag(landing_container, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(landing_container, LV_OBJ_FLAG_HIDDEN);
    }
}