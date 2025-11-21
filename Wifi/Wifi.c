#include "Wifi.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "Medicine.h"

static const char *TAG = "Wifi";

uint8_t wifi_connect_fail_count = 0;
bool wifi_connected = false;
bool wifi_running_smartconfig = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "WiFi STA started");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_connected = false;

        if (!wifi_running_smartconfig)
        {
            wifi_connect_fail_count++;

            if (wifi_connect_fail_count < 5)
            {
                ESP_LOGW(TAG, "Disconnected, retrying...");
                esp_wifi_connect();
            }
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        wifi_connect_fail_count = 0;
        wifi_connected = true;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        MedicineMQTT_Start();
    }
}

static void smartconfig_event_handler(void *arg, esp_event_base_t event_base,
                                      int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case SC_EVENT_SCAN_DONE:
        ESP_LOGI(TAG, "SC_EVENT_SCAN_DONE");
        break;

    case SC_EVENT_FOUND_CHANNEL:
        ESP_LOGI(TAG, "SC_EVENT_FOUND_CHANNEL");
        break;

    case SC_EVENT_GOT_SSID_PSWD:
    {
        ESP_LOGI(TAG, "SC_EVENT_GOT_SSID_PSWD");

        smartconfig_event_got_ssid_pswd_t *evt =
            (smartconfig_event_got_ssid_pswd_t *)event_data;

        wifi_config_t wifi_config = {0};

        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));

        ESP_LOGI(TAG, "NEW SSID: %s", wifi_config.sta.ssid);
        ESP_LOGI(TAG, "NEW PASS: %s", wifi_config.sta.password);

        // Save to NVS
        nvs_handle_t nvs;
        nvs_open("wifi", NVS_READWRITE, &nvs);
        nvs_set_str(nvs, "ssid", (char *)wifi_config.sta.ssid);
        nvs_set_str(nvs, "pass", (char *)wifi_config.sta.password);
        nvs_commit(nvs);
        nvs_close(nvs);

        // Apply config
        esp_wifi_disconnect();
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        esp_wifi_connect();

        break;
    }

    case SC_EVENT_SEND_ACK_DONE:
        ESP_LOGI(TAG, "SmartConfig finished");
        esp_smartconfig_stop();
        wifi_running_smartconfig = false;
        break;
    }
}

void wifi_start_smartconfig()
{
    wifi_running_smartconfig = true;

    ESP_LOGW(TAG, "Starting SmartConfig...");

    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(500));

    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    esp_smartconfig_start(&cfg);
}

bool Wifi_LoadFromNVS(char *ssid, char *pass)
{
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READONLY, &nvs) != ESP_OK)
        return false;

    size_t ssid_len = 32;
    size_t pass_len = 64;

    if (nvs_get_str(nvs, "ssid", ssid, &ssid_len) != ESP_OK)
        return false;
    if (nvs_get_str(nvs, "pass", pass, &pass_len) != ESP_OK)
        return false;

    nvs_close(nvs);
    return true;
}

void wifi_fallback_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(8000));

    if (!wifi_connected)
    {
        wifi_start_smartconfig();
    }

    vTaskDelete(NULL);
}

void Wifi_Init(void)
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(SC_EVENT, ESP_EVENT_ANY_ID, smartconfig_event_handler, NULL, NULL);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    char ssid[32] = {0}, pass[64] = {0};

    if (Wifi_LoadFromNVS(ssid, pass))
    {
        ESP_LOGI(TAG, "Found saved WiFi: %s", ssid);

        wifi_config_t wifi_config = {0};
        strcpy((char *)wifi_config.sta.ssid, ssid);
        strcpy((char *)wifi_config.sta.password, pass);

        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        esp_wifi_connect();

        xTaskCreate(wifi_fallback_task, "wifi_fallback", 2048, NULL, 5, NULL);
    }
}
