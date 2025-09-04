#ifndef WIFI_SUCCESS_UI_H
#define WIFI_SUCCESS_UI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void create_wifi_success_ui(lv_obj_t * parent);
void wifi_success_ui_show(bool show);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_SUCCESS_UI_H */