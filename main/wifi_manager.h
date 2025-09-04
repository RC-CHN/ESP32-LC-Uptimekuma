#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

/**
 * @brief WiFi manager status enumeration.
 */
#include "esp_netif.h"

typedef enum {
    WIFI_STATUS_DISCONNECTED,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
} wifi_manager_status_t;

/**
 * @brief Callback function type for WiFi status changes.
 *
 * @param status The new WiFi status.
 */
typedef void (*wifi_manager_status_cb_t)(wifi_manager_status_t status);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the WiFi manager.
 */
void wifi_manager_init(void);

/**
 * @brief Tries to load WiFi configuration from NVS and connect.
 *
 * @return true if a configuration was found and a connection attempt was started.
 * @return false if no configuration was found in NVS.
 */
bool wifi_manager_load_sta_config_and_connect(void);

/**
 * @brief Connect to a WiFi network with the given credentials and save them to NVS on success.
 *
 * @param ssid The SSID of the network.
 * @param password The password of the network.
 */
void wifi_manager_connect(const char *ssid, const char *password);

/**
 * @brief Set the callback function for WiFi status changes.
 *
 * @param cb The callback function to be called on status change.
 */
void wifi_manager_set_status_callback(wifi_manager_status_cb_t cb);

/**
 * @brief Get the current station IP information.
 *
 * @param ip_info Pointer to a structure to store the IP information.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t wifi_manager_get_ip_info(esp_netif_ip_info_t* ip_info);

/**
 * @brief Get the current station SSID.
 *
 * @param ssid Buffer to store the SSID.
 * @param len Length of the buffer.
 */
void wifi_manager_get_ssid(char* ssid, size_t len);

/**
 * @brief Clear the stored WiFi configuration from NVS.
 *
 * @return esp_err_t ESP_OK on success, or an error from NVS.
 */
esp_err_t wifi_manager_clear_config(void);


#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H */