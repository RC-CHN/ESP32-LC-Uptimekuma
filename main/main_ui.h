#ifndef MAIN_UI_H
#define MAIN_UI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void create_main_ui(lv_obj_t *parent);
void main_ui_show(bool show);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*MAIN_UI_H*/