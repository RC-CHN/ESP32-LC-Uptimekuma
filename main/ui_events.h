#ifndef UI_EVENTS_H
#define UI_EVENTS_H

#include "lvgl.h"

// Define custom events for UI interaction
#define EVT_SWITCH_TO_CONFIG_UI (_LV_EVENT_LAST + 1)
#define EVT_SWITCH_TO_MAIN_APP  (_LV_EVENT_LAST + 2)
#define EVT_SWITCH_TO_KUMA_UI   (_LV_EVENT_LAST + 3)

#endif /* UI_EVENTS_H */