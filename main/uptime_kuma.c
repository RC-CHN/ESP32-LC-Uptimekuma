#include <string.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "uptime_kuma.h"

#define KUMA_BASE_URL "http://uptime.cloud.rcfortress.site"
static const char *TAG = "KUMA_CLIENT";

#define HEARBEAT_PARSER_BUFFER_SIZE (33 * 1024) // 33KB, for large heartbeat data

// State for the streaming heartbeat parser
typedef struct {
    kuma_monitor_list_t *monitor_list;
    char *buffer;
    int buffer_len;
} heartbeat_parser_state_t;


// HTTP event handler for the main JSON response (non-streaming)
static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (evt->user_data) {
                char **response_buffer = (char **)evt->user_data;
                int old_len = *response_buffer ? strlen(*response_buffer) : 0;
                *response_buffer = realloc(*response_buffer, old_len + evt->data_len + 1);
                memcpy(*response_buffer + old_len, evt->data, evt->data_len);
                (*response_buffer)[old_len + evt->data_len] = '\0';
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

// HTTP event handler for streaming the heartbeat response
static esp_err_t _heartbeat_stream_event_handler(esp_http_client_event_t *evt) {
    heartbeat_parser_state_t *state = (heartbeat_parser_state_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_ON_DATA: {
            if (state->buffer_len + evt->data_len < HEARBEAT_PARSER_BUFFER_SIZE) {
                memcpy(state->buffer + state->buffer_len, evt->data, evt->data_len);
                state->buffer_len += evt->data_len;
                state->buffer[state->buffer_len] = '\0';
            } else {
                ESP_LOGE(TAG, "Heartbeat parser buffer overflow!");
                state->buffer_len = 0;
                return ESP_FAIL;
            }
        }
        break;
        case HTTP_EVENT_ON_FINISH: {
            ESP_LOGI(TAG, "ON_FINISH: Total received: %d bytes. Parsing now.", state->buffer_len);

            char *heartbeat_list_start = strstr(state->buffer, "\"heartbeatList\":{");
            if (!heartbeat_list_start) {
                ESP_LOGE(TAG, "Could not find 'heartbeatList' in response.");
                return ESP_FAIL;
            }
            heartbeat_list_start += strlen("\"heartbeatList\":{");

            for (int i = 0; i < state->monitor_list->count; i++) {
                kuma_monitor_t *m = state->monitor_list->monitors[i];
                if (!m) continue;

                // Create the search key for the monitor, e.g., "\"1\":"
                char key[20];
                snprintf(key, sizeof(key), "\"%d\":", m->id);

                char *monitor_array_start = strstr(heartbeat_list_start, key);
                if (!monitor_array_start) continue;

                monitor_array_start += strlen(key);
                if (*monitor_array_start != '[') continue; // Should be an array

                // Find the matching closing bracket for this array
                int bracket_level = 0;
                char *monitor_array_end = monitor_array_start;
                do {
                    if (*monitor_array_end == '[') bracket_level++;
                    else if (*monitor_array_end == ']') bracket_level--;
                    monitor_array_end++;
                } while (bracket_level > 0 && *monitor_array_end != '\0');

                if (bracket_level != 0) continue; // Malformed JSON

                // Temporarily null-terminate this small JSON array string
                char original_char = *monitor_array_end;
                *monitor_array_end = '\0';

                cJSON *heartbeat_array = cJSON_Parse(monitor_array_start);
                if (heartbeat_array && cJSON_IsArray(heartbeat_array)) {
                     int array_size = cJSON_GetArraySize(heartbeat_array);
                    if (array_size > 0) {
                        cJSON *latest_heartbeat = cJSON_GetArrayItem(heartbeat_array, array_size - 1);
                        cJSON *status_json = cJSON_GetObjectItem(latest_heartbeat, "status");
                        cJSON *ping_json = cJSON_GetObjectItem(latest_heartbeat, "ping");

                        if (cJSON_IsNumber(status_json)) {
                            m->status = (status_json->valueint == 1) ? KUMA_MONITOR_STATUS_UP : KUMA_MONITOR_STATUS_DOWN;
                        }
                        if (cJSON_IsNumber(ping_json)) {
                            m->ping = ping_json->valueint;
                        }
                        ESP_LOGI(TAG, "Final Parse: Monitor %d: status=%d, ping=%d", m->id, m->status, m->ping);
                    }
                }
                if(heartbeat_array) cJSON_Delete(heartbeat_array);

                // Restore the original character
                *monitor_array_end = original_char;
            }
        }
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HEARTBEAT_STREAM_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

// Internal function to perform a standard, non-streaming HTTP GET request
static char* _http_perform_get(const char *url) {
    char *response_buffer = NULL;

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        .user_data = &response_buffer,
        .disable_auto_redirect = true,
        .buffer_size = 2048, // Increase buffer size for large JSON
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld",
                 status_code,
                 esp_http_client_get_content_length(client));
        if (status_code != 200) {
            ESP_LOGE(TAG, "HTTP request failed with status code %d", status_code);
            free(response_buffer);
            response_buffer = NULL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        free(response_buffer);
        response_buffer = NULL;
    }
    esp_http_client_cleanup(client);
    return response_buffer;
}

void kuma_client_free_monitor_list(kuma_monitor_list_t *list) {
    if (!list) {
        return;
    }
    if (list->monitors) {
        for (int i = 0; i < list->count; i++) {
            if (list->monitors[i]) {
                free(list->monitors[i]->name);
                free(list->monitors[i]);
            }
        }
        free(list->monitors);
    }
    free(list);
}

esp_err_t kuma_client_fetch_data(const char *slug, kuma_monitor_list_t **out_list) {
    //
    // 这个函数的主体逻辑会很长，我们会分几步来完成：
    // 1. 获取 status-page 数据
    // 2. 解析 status-page JSON，获取 monitor ID 和 name
    // 3. 获取 heartbeat 数据
    // 4. 解析 heartbeat JSON，获取 status 和 ping
    // 5. 合并数据，创建 kuma_monitor_list_t
    //
    // 目前，我们先实现第一步。
    char url[256];
    snprintf(url, sizeof(url), "%s/api/status-page/%s", KUMA_BASE_URL, slug);

    ESP_LOGI(TAG, "Fetching monitor list from: %s", url);
    char *status_page_json_str = _http_perform_get(url);

    if (!status_page_json_str) {
        ESP_LOGE(TAG, "Failed to get status page data.");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Got status page response: %s", status_page_json_str);

    cJSON *root = cJSON_Parse(status_page_json_str);
    free(status_page_json_str); // 无论如何都释放字符串
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse status page JSON.");
        return ESP_FAIL;
    }

    cJSON *publicGroupList = cJSON_GetObjectItem(root, "publicGroupList");
    if (!cJSON_IsArray(publicGroupList)) {
        ESP_LOGE(TAG, "publicGroupList is not an array.");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // 计算总的 monitor 数量
    int total_monitors = 0;
    cJSON *group = NULL;
    cJSON_ArrayForEach(group, publicGroupList) {
        cJSON *monitorList = cJSON_GetObjectItem(group, "monitorList");
        if (cJSON_IsArray(monitorList)) {
            total_monitors += cJSON_GetArraySize(monitorList);
        }
    }

    if (total_monitors == 0) {
        ESP_LOGI(TAG, "No monitors found on the status page.");
        cJSON_Delete(root);
        *out_list = NULL;
        return ESP_OK;
    }

    // 为 monitor 列表分配内存
    kuma_monitor_list_t *list = calloc(1, sizeof(kuma_monitor_list_t));
    list->monitors = calloc(total_monitors, sizeof(kuma_monitor_t *));
    list->count = total_monitors;
    
    if (!list->monitors) {
        ESP_LOGE(TAG, "Failed to allocate memory for monitors.");
        free(list);
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    int current_monitor_index = 0;
    cJSON_ArrayForEach(group, publicGroupList) {
        cJSON *monitorList = cJSON_GetObjectItem(group, "monitorList");
        cJSON *monitor_json = NULL;
        cJSON_ArrayForEach(monitor_json, monitorList) {
            kuma_monitor_t *monitor = calloc(1, sizeof(kuma_monitor_t));
            if (!monitor) continue; // 分配失败，跳过

            cJSON *id_json = cJSON_GetObjectItem(monitor_json, "id");
            cJSON *name_json = cJSON_GetObjectItem(monitor_json, "name");

            if (cJSON_IsNumber(id_json) && cJSON_IsString(name_json)) {
                monitor->id = id_json->valueint;
                monitor->name = strdup(name_json->valuestring);
                monitor->status = KUMA_MONITOR_STATUS_UNKNOWN; // 默认状态
                monitor->ping = 0;
                list->monitors[current_monitor_index++] = monitor;
            } else {
                free(monitor);
            }
        }
    }
    cJSON_Delete(root);

    // 如果没有 monitor，直接返回
    if (current_monitor_index == 0) {
        *out_list = list;
        return ESP_OK;
    }

    // --- 第二步：获取并解析 Heartbeat 数据 ---
    snprintf(url, sizeof(url), "%s/api/status-page/heartbeat/%s", KUMA_BASE_URL, slug);
    ESP_LOGI(TAG, "Fetching heartbeat data from: %s (streaming)", url);

    // Initialize the parser state
    // Initialize the parser state
    heartbeat_parser_state_t parser_state = {
        .monitor_list = list,
        .buffer_len = 0,
        .buffer = NULL,
    };

    // Dynamically allocate the buffer for the heartbeat response
    parser_state.buffer = malloc(HEARBEAT_PARSER_BUFFER_SIZE);
    if (parser_state.buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for heartbeat buffer");
        // We can still return the list of monitors, just without status/ping data.
        *out_list = list;
        return ESP_ERR_NO_MEM;
    }
    memset(parser_state.buffer, 0, HEARBEAT_PARSER_BUFFER_SIZE);


    esp_http_client_config_t http_cfg = {
        .url = url,
        .event_handler = _heartbeat_stream_event_handler,
        .user_data = &parser_state,
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t http_client = esp_http_client_init(&http_cfg);
    esp_err_t err = esp_http_client_perform(http_client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get heartbeat data stream: %s", esp_err_to_name(err));
        // Even if streaming fails, we return the list without status info
    }
    
    esp_http_client_cleanup(http_client);

    // Free the dynamically allocated buffer
    free(parser_state.buffer);

    // --- Data is now parsed within the event handler ---
    // The old parsing logic is now obsolete. We will replace it later.
    // For now, we just return.

    // The old cJSON parsing logic for heartbeat is now obsolete
    // and has been removed. The new logic will be implemented
    // inside the _heartbeat_stream_event_handler.

    *out_list = list;
    return ESP_OK;
}