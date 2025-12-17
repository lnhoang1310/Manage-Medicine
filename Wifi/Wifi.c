#include "Wifi.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "Medicine.h"
#include "esp_http_server.h"

static const char *TAG = "Wifi";

bool wifi_connected = false;
bool mqtt_connected = false;

static esp_netif_t *ap_netif = NULL;
static esp_netif_t *sta_netif = NULL;
static httpd_handle_t server = NULL;

/* ======================================================
   HTTP HANDLER: /wifi  (App gửi thông tin WiFi vào)
   ====================================================== */
static esp_err_t wifi_post_handler(httpd_req_t *req)
{
    char buf[200];
    int len = httpd_req_recv(req, buf, sizeof(buf));
    if (len <= 0)
        return ESP_FAIL;

    buf[len] = 0;
    ESP_LOGI(TAG, "Received WiFi JSON: %s", buf);

    char ssid[32] = {0};
    char pass[64] = {0};

    sscanf(buf, "{\"ssid\":\"%31[^\"]\",\"pass\":\"%63[^\"]\"}", ssid, pass);

    ESP_LOGW(TAG, "Provision SSID=%s PASS=%s", ssid, pass);

    // Save to NVS
    nvs_handle_t nvs;
    nvs_open("wifi", NVS_READWRITE, &nvs);
    nvs_set_str(nvs, "ssid", ssid);
    nvs_set_str(nvs, "pass", pass);
    nvs_commit(nvs);
    nvs_close(nvs);

    // Send response
    httpd_resp_sendstr(req, "OK");

    // Switch to STA mode
    wifi_config_t cfg = {0};
    strcpy((char *)cfg.sta.ssid, ssid);
    strcpy((char *)cfg.sta.password, pass);

    ESP_LOGI(TAG, "Switching to STA...");
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_start();
    esp_wifi_connect();

    return ESP_OK;
}

/* ======================================================
   Start HTTP Server
   ====================================================== */
httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t wifi_uri = {
            .uri = "/wifi",
            .method = HTTP_POST,
            .handler = wifi_post_handler,
        };
        httpd_register_uri_handler(server, &wifi_uri);

        ESP_LOGI(TAG, "HTTP Server started");
    }
    return server;
}

/* ======================================================
   Start AP for provisioning
   ====================================================== */
void wifi_start_ap(void)
{
    ESP_LOGW(TAG, "Starting AP Provisioning...");

    if (server)
    {
        httpd_stop(server);
        server = NULL;
    }

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = "ESP_SETUP",
            .password = "12345678",
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();

    server = start_webserver();

    ESP_LOGI(TAG, "AP ready: SSID=ESP_SETUP PASS=12345678");
}

/* ======================================================
   Event Handler WiFi
   ====================================================== */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_STA_START)
        {
            esp_wifi_connect();
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            mqtt_connected = false;
            wifi_connected = false;
            ESP_LOGW(TAG, "WiFi disconnected → switching back AP");
            wifi_start_ap();
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        wifi_connected = true;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        if (!mqtt_connected)
        {
            mqtt_connected = true;
            MedicineMQTT_Start();
        }
    }
}

/* ======================================================
   Load from NVS
   ====================================================== */
bool Wifi_LoadFromNVS(char *ssid, char *pass)
{
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READONLY, &nvs) != ESP_OK)
        return false;

    size_t ssid_len = 32, pass_len = 64;
    if (nvs_get_str(nvs, "ssid", ssid, &ssid_len) != ESP_OK)
        return false;
    if (nvs_get_str(nvs, "pass", pass, &pass_len) != ESP_OK)
        return false;

    nvs_close(nvs);
    return true;
}

/* ======================================================
   Init WiFi
   ====================================================== */
void Wifi_Init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi...");

    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    ap_netif = esp_netif_create_default_wifi_ap();
    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL);

    char ssid[32] = {0};
    char pass[64] = {0};

    if (Wifi_LoadFromNVS(ssid, pass))
    {
        wifi_config_t sta_cfg = {0};
        strcpy((char *)sta_cfg.sta.ssid, ssid);
        strcpy((char *)sta_cfg.sta.password, pass);

        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
        esp_wifi_start();
        esp_wifi_connect();
    }
    else
    {
        wifi_start_ap();
    }
}
