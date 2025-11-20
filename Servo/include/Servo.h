#ifndef SERVO_H
#define SERVO_H

#include "driver/ledc.h"

#define SERVO_NUM 4
#define SERVO_MIN_PULSE_WIDTH_US 450
#define SERVO_MAX_PULSE_WIDTH_US 2500
#define SERVO_FREQ_HZ 50       
#define SERVO1_TIMER LEDC_TIMER_0
#define SERVO1_CHANNEL LEDC_CHANNEL_0
#define SERVO1_GPIO 1
#define SERVO2_TIMER LEDC_TIMER_1
#define SERVO2_CHANNEL LEDC_CHANNEL_1
#define SERVO2_GPIO 2
#define SERVO3_TIMER LEDC_TIMER_2
#define SERVO3_CHANNEL LEDC_CHANNEL_2
#define SERVO3_GPIO 3
#define SERVO4_TIMER LEDC_TIMER_3
#define SERVO4_CHANNEL LEDC_CHANNEL_3
#define SERVO4_GPIO 4


typedef struct{
    ledc_channel_t channel;
    ledc_timer_t timer;
    uint32_t freq_hz;
    int gpio_num;
    uint32_t min_pulse_width_us;
    uint32_t max_pulse_width_us;
    float angle;
}Servo_Typedef;

extern Servo_Typedef* servo_list[SERVO_NUM];

void Servo_Init(Servo_Typedef* servo, ledc_timer_t timer, ledc_channel_t channel, int gpio_num);
void Servo_SetAngle(Servo_Typedef* servo, float angle);

#endif
