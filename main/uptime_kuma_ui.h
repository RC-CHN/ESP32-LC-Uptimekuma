#ifndef UPTIME_KUMA_UI_H
#define UPTIME_KUMA_UI_H

#include "lvgl.h"
#include "uptime_kuma.h"

/**
 * @brief 创建并显示 Uptime Kuma 状态监控界面
 *
 * @param parent 父对象
 */
void create_uptime_kuma_ui(lv_obj_t *parent);

/**
 * @brief 使用新的监控列表数据更新UI
 *
 * @param monitor_list 最新的监控数据列表
 */
void update_uptime_kuma_ui(const kuma_monitor_list_t *monitor_list);

#endif // UPTIME_KUMA_UI_H