#include "Medicine.h"
#include "Wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "Main";

// Khai báo servo và thuốc
Servo_Typedef servo1, servo2, servo3, servo4;
Drug_Typedef drugs[DRUG_COUNT] = {
    {.name = "Paracetamol 500mg", .nums = 0, .servo = &servo1},
    {.name = "Paracetamol 600mg", .nums = 0, .servo = &servo2},
    {.name = "Paracetamol 700mg", .nums = 0, .servo = &servo3},
    {.name = "D", .nums = 0, .servo = &servo4}};

// Callback khi có dữ liệu MQTT
void medicin_callback(Drug_Typedef drugs[DRUG_COUNT])
{
    for (int i = 0; i < DRUG_COUNT; i++)
    {
        ESP_LOGI("MedicineCallback", "Drug: %s, Nums: %d", drugs[i].name, drugs[i].nums);
    }
}

// Task xử lý servo
void servo_task(void *pvParameters)
{
    while (1)
    {
        if (Medicine_GetFlagControlServo())
        {
            for (uint8_t i = 0; i < DRUG_COUNT; i++)
            {
                uint8_t nums = drugs[i].nums; // copy ra để không thay đổi gốc
                while (nums--)
                {
                    for(int16_t j=179; j>=0; j--){
                        Servo_SetAngle(drugs[i].servo, (float)j);
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    Servo_SetAngle(drugs[i].servo, 180.0f);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    ESP_LOGI(TAG, "Dispensed one unit of %s", drugs[i].name);
                }
                drugs[i].nums = 0; // reset sau khi đã cấp phát
            }
            Medicine_ClearFlagControlServo();
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // yield
    }
}

void app_main(void)
{
    // Wifi
    Wifi_Init(WIFI_SSID, WIFI_PASSWORD);

    // Khởi tạo servo
    Servo_Init(&servo1, SERVO1_TIMER, SERVO1_CHANNEL, SERVO1_GPIO);
    Servo_Init(&servo2, SERVO2_TIMER, SERVO2_CHANNEL, SERVO2_GPIO);
    Servo_Init(&servo3, SERVO3_TIMER, SERVO3_CHANNEL, SERVO3_GPIO);
    Servo_Init(&servo4, SERVO4_TIMER, SERVO4_CHANNEL, SERVO4_GPIO);

    // Khởi tạo MQTT nhưng chưa start
    MedicineMQTT_Init(MQTT_BROKER_URI, MQTT_TOPIC_SUB, drugs);
    MedicineMQTT_SetCallback(medicin_callback);
    // **Không gọi MedicineMQTT_Start() ở đây**
    // MQTT sẽ start trong wifi_event_handler khi ESP32 got IP

    // Tạo task xử lý servo
    xTaskCreate(servo_task, "servo_task", 4096, NULL, 2, NULL);

    ESP_LOGI(TAG, "App initialized. Waiting for WiFi and MQTT...");
}
