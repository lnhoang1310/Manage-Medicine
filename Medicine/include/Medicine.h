#ifndef MEDICINE_H
#define MEDICINE_H

#include "mqtt_client.h"
#include "Servo.h"
#include <stdbool.h>

#define MAX_LEN_NAME 100
#define DRUG_COUNT 3
#define MQTT_BROKER_URI "mqtts://medicine-mqtt.huy.lat:8883"
#define MQTT_TOPIC_SUB "medicine/control"

typedef struct
{
    char name[MAX_LEN_NAME];
    uint8_t nums;
    Servo_Typedef *servo;
} Drug_Typedef;

typedef void (*MedicineCallback_t)(Drug_Typedef drugs[DRUG_COUNT]);
void MedicineMQTT_Init(const char *broker_uri, const char *topic_sub, Drug_Typedef drugs[DRUG_COUNT]);
void MedicineMQTT_SetCallback(MedicineCallback_t cb);
void MedicineMQTT_Start(void);
bool Medicine_GetFlagControlServo(void);
void Medicine_ClearFlagControlServo(void);

#endif