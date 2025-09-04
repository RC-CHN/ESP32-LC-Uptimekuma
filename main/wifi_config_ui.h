#ifndef WIFI_CONFIG_UI_H
#define WIFI_CONFIG_UI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void create_wifi_config_ui(lv_disp_t *disp);
void wifi_config_ui_show(bool show);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_CONFIG_UI_H */