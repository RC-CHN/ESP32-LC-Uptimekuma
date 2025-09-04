#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "sdkconfig.h" // Include sdkconfig for CONFIG_ variables
#include "esp_wifi.h"
#include <string.h>

#define WIFI_STORAGE_NAMESPACE "wifi_storage"
#define WIFI_SSID_KEY "ssid"
#define WIFI_PASS_KEY "password"

static const char *TAG = "wifi_manager";
static char s_ssid[32];
static char s_password[64];
static char s_current_ssid[32];
static esp_netif_ip_info_t s_ip_info;
static wifi_manager_status_cb_t s_status_cb = NULL;

// Forward declarations
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static esp_err_t save_wifi_config(const char *ssid, const char *password);

void wifi_manager_set_status_callback(wifi_manager_status_cb_t cb)
{
    s_status_cb = cb;
}

esp_err_t wifi_manager_get_ip_info(esp_netif_ip_info_t* ip_info)
{
    if (ip_info) {
        *ip_info = s_ip_info;
        return ESP_OK;
    }
    return ESP_FAIL;
}

void wifi_manager_get_ssid(char* ssid, size_t len)
{
    strncpy(ssid, s_current_ssid, len);
    ssid[len-1] = '\0';
}

esp_err_t wifi_manager_clear_config(void)
{
    ESP_LOGI(TAG, "Clearing Wi-Fi credentials from NVS...");
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return err;
    }
    
    // Erase the keys
    err = nvs_erase_key(nvs_handle, WIFI_SSID_KEY);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
         ESP_LOGE(TAG, "Failed to erase SSID");
    }
    
    err = nvs_erase_key(nvs_handle, WIFI_PASS_KEY);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
         ESP_LOGE(TAG, "Failed to erase Password");
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS");
    } else {
        ESP_LOGI(TAG, "WiFi config cleared from NVS successfully.");
    }

    nvs_close(nvs_handle);
    return err;
}

void wifi_manager_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
}

bool wifi_manager_load_sta_config_and_connect(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "WiFi configuration not found in NVS. This is normal on first boot.");
        } else {
            ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        }
        return false;
    }

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    size_t required_size = sizeof(wifi_config.sta.ssid);
    err = nvs_get_str(nvs_handle, WIFI_SSID_KEY, (char *)wifi_config.sta.ssid, &required_size);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No SSID found in NVS");
        nvs_close(nvs_handle);
        return false;
    }

    required_size = sizeof(wifi_config.sta.password);
    err = nvs_get_str(nvs_handle, WIFI_PASS_KEY, (char *)wifi_config.sta.password, &required_size);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No Password found in NVS");
        nvs_close(nvs_handle);
        return false;
    }

    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Found WiFi config in NVS. SSID: %s", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    if (s_status_cb) s_status_cb(WIFI_STATUS_CONNECTING);
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect: %s", esp_err_to_name(ret));
        return false;
    }
    return true;
}

void wifi_manager_connect(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid);
    
    // Store credentials to be saved upon successful connection
    strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
    s_ssid[sizeof(s_ssid) - 1] = '\0';
    strncpy(s_password, password, sizeof(s_password) - 1);
    s_password[sizeof(s_password) - 1] = '\0';

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, password);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    if (s_status_cb) s_status_cb(WIFI_STATUS_CONNECTING);
    esp_wifi_connect();
}


static esp_err_t save_wifi_config(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "Saving Wi-Fi credentials to NVS...");
    ESP_LOGI(TAG, "SSID: %s", ssid);
    ESP_LOGI(TAG, "Password: %s", password);

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(nvs_handle, WIFI_SSID_KEY, ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write SSID to NVS");
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_str(nvs_handle, WIFI_PASS_KEY, password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write Password to NVS");
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS");
    } else {
        ESP_LOGI(TAG, "WiFi config committed to NVS successfully.");
    }

    nvs_close(nvs_handle);
    return err;
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
        //esp_wifi_connect(); // This will be called by the functions above
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* disconnected_event = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "WIFI_EVENT_STA_DISCONNECTED: Connect to the AP fail. Reason: %d. Retrying...", disconnected_event->reason);
        memset(&s_ip_info, 0, sizeof(s_ip_info));
        memset(s_current_ssid, 0, sizeof(s_current_ssid));
        if (s_status_cb) s_status_cb(WIFI_STATUS_DISCONNECTED);
        // ESP-IDF will automatically retry to connect. No need to call esp_wifi_connect() here.
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP: Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        
        s_ip_info = event->ip_info;
        
        wifi_config_t wifi_config;
        esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
        strncpy(s_current_ssid, (const char*)wifi_config.sta.ssid, sizeof(s_current_ssid) - 1);
        s_current_ssid[sizeof(s_current_ssid) - 1] = '\0';


        if (s_status_cb) s_status_cb(WIFI_STATUS_CONNECTED);
        // Connection successful. Save the credentials if they were set by wifi_manager_connect
        if (strlen(s_ssid) > 0) {
            save_wifi_config(s_ssid, s_password);
            // Clear temporary credentials
            memset(s_ssid, 0, sizeof(s_ssid));
            memset(s_password, 0, sizeof(s_password));
        }
    }
}