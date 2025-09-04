#ifndef UPTIME_KUMA_H
#define UPTIME_KUMA_H

#include "esp_err.h"

// 定义监控项状态
typedef enum {
    KUMA_MONITOR_STATUS_UNKNOWN,
    KUMA_MONITOR_STATUS_UP,
    KUMA_MONITOR_STATUS_DOWN,
} kuma_monitor_status_t;

// 定义单个监控项的数据结构
typedef struct {
    int id;
    char *name;
    kuma_monitor_status_t status;
    int ping;
} kuma_monitor_t;

// 定义存储所有监控项的列表
typedef struct {
    kuma_monitor_t **monitors;
    int count;
} kuma_monitor_list_t;

/**
 * @brief 从 Uptime Kuma API 获取并解析监控状态
 *
 * @param slug 状态页面的 slug
 * @param[out] out_list 函数成功后，将通过此指针返回一个包含所有监控项信息的列表。
 *                      调用者有责任在使用完毕后调用 kuma_client_free_monitor_list 来释放内存。
 * @return
 *      - ESP_OK: 成功
 *      - 其他: 失败
 */
esp_err_t kuma_client_fetch_data(const char *slug, kuma_monitor_list_t **out_list);

/**
 * @brief 释放由 kuma_client_fetch_data 创建的监控列表
 *
 * @param list 要释放的监控列表
 */
void kuma_client_free_monitor_list(kuma_monitor_list_t *list);

#endif // UPTIME_KUMA_H