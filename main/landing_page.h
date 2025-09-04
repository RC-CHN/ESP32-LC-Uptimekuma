#ifndef LANDING_PAGE_H
#define LANDING_PAGE_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the landing page UI elements.
 *
 * @param parent The parent object for the UI elements.
 */
void create_landing_page_ui(lv_obj_t *parent);

/**
 * @brief Show or hide the landing page.
 *
 * @param show true to show, false to hide.
 */
void landing_page_ui_show(bool show);

#ifdef __cplusplus
}
#endif

#endif /* LANDING_PAGE_H */