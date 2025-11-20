#include "Medicine.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "MedicineMQTT";

static esp_mqtt_client_handle_t client = NULL;
static MedicineCallback_t user_cb = NULL;
static Drug_Typedef *drug_array = NULL;
static char topic_sub_global[128];
static volatile bool flag_control_servo = false;
extern const uint8_t isrgrootx1_pem_start[] asm("_binary_isrgrootx1_pem_start");
extern const uint8_t isrgrootx1_pem_end[]   asm("_binary_isrgrootx1_pem_end");

// Set callback để main nhận dữ liệu
void MedicineMQTT_SetCallback(MedicineCallback_t cb)
{
    user_cb = cb;
}

bool Medicine_GetFlagControlServo(void)
{
    return flag_control_servo;
}
void Medicine_ClearFlagControlServo(void)
{
    flag_control_servo = false;
}

// Parse JSON và gán vào mảng drugs
static void parse_json(const char *data)
{
    if (!drug_array)
        return;

    cJSON *root = cJSON_Parse(data);
    if (!root)
    {
        ESP_LOGE(TAG, "JSON parse failed");
        return;
    }

    for (int i = 0; i < DRUG_COUNT; i++)
    {
        cJSON *item = cJSON_GetObjectItem(root, drug_array[i].name);
        if (item)
        {
            drug_array[i].nums = (uint8_t)item->valueint;
        }
    }

    flag_control_servo = true;

    if (user_cb)
        user_cb(drug_array);

    cJSON_Delete(root);
}

// MQTT event handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected, subscribing topic");
        esp_mqtt_client_subscribe(client, topic_sub_global, 0);
        break;
    case MQTT_EVENT_DATA:
        char buf[256];
        memset(buf, 0, sizeof(buf));
        memcpy(buf, event->data, event->data_len);
        ESP_LOGI(TAG, "MQTT DATA: %s", buf);

        parse_json(buf);
        break;
    default:
        break;
    }
}

// Khởi tạo MQTT với broker + topic + mảng drugs
void MedicineMQTT_Init(const char *broker_uri, const char *topic_sub, Drug_Typedef drugs[DRUG_COUNT])
{
    strncpy(topic_sub_global, topic_sub, sizeof(topic_sub_global) - 1);
    drug_array = drugs;

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
        .broker.verification.certificate = (const char *)isrgrootx1_pem_start,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
}

// Start MQTT
void MedicineMQTT_Start(void)
{
    if (client)
        esp_mqtt_client_start(client);
}
