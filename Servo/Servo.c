#include "Servo.h"

Servo_Typedef *servo_list[SERVO_NUM];
static uint8_t servo_index = 0;

void Servo_Init(Servo_Typedef *servo, ledc_timer_t timer, ledc_channel_t channel, int gpio_num)
{
    servo->channel = channel;
    servo->timer = timer;
    servo->freq_hz = SERVO_FREQ_HZ;
    servo->min_pulse_width_us = SERVO_MIN_PULSE_WIDTH_US;
    servo->max_pulse_width_us = SERVO_MAX_PULSE_WIDTH_US;
    servo->angle = 0.0f;
    servo->gpio_num = gpio_num;
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = servo->timer,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .freq_hz = servo->freq_hz,
        .clk_cfg = LEDC_AUTO_CLK};
    ledc_timer_config(&ledc_timer);
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = servo->channel,
        .timer_sel = servo->timer,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = gpio_num,
        .duty = 0,
        .hpoint = 0};
    ledc_channel_config(&ledc_channel);
    servo_list[servo_index++] = servo;
    if (servo_index >= 4)
        servo_index = 0;
    Servo_SetAngle(servo, 180.0f);
}

void Servo_SetAngle(Servo_Typedef *servo, float angle)
{
    if (angle < 0.0f)
        angle = 0.0f;
    if (angle > 200.0f)
        angle = 200.0f;

    servo->angle = angle;

    uint32_t period_us = 1000000 / servo->freq_hz;
    uint32_t max_duty = (1 << 14) - 1;

    uint32_t pulse_width_us = servo->min_pulse_width_us + (angle / 180.0f) * (servo->max_pulse_width_us - servo->min_pulse_width_us);

    uint32_t duty = (pulse_width_us * max_duty) / period_us;

    ledc_set_duty(LEDC_LOW_SPEED_MODE, servo->channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, servo->channel);
}
