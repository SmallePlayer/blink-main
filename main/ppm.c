#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"
#include "driver/ledc.h"

#define LED_GPIO 21



int hue = 0;
led_strip_handle_t led_config(){
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,                  // один светодиод на плате
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10 МГц
        .flags.with_dma = false,
    };
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
    return led_strip;
}
void led_on_chip(led_strip_handle_t led_strip){
    led_strip_set_pixel_hsv(led_strip, 0, hue, 255, 16);
    led_strip_refresh(led_strip);

    hue++;                      // плавно двигаем оттенок
    if (hue >= 360) {
        hue = 0;                // зацикливаем
    }

    vTaskDelay(pdMS_TO_TICKS(10));   // скорость анимации
}


void setting_pwm_pin(int pin, int freq, int resolution, int pin_channel){
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,  // режим скорости
        .timer_num       = LEDC_TIMER_0,         // какой таймер (0..3)
        .duty_resolution = resolution,     // разрешение
        .freq_hz         = freq,                  // частота
        .clk_cfg         = LEDC_AUTO_CLK,        // источник тактов
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {
        .gpio_num       = pin,
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = pin_channel,
        .timer_sel      = LEDC_TIMER_0,
        .duty           = 0, // начальная скважность
    };
    ledc_channel_config(&channel);
}

void pwm_pin(int pin, int variable){
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, variable);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

int pwm_duty(int pwm_signal){
    return pwm_signal * (1 << PWM_RES) / (1000000 / PWM_FREQ); // 20 мс = 20000 мкс, 16384 = 2^14 (разрешение 14 бит)
}


#define LED_PIN 2

void app_main(void)
{
    int output = (int)round((input + 100.0) * 5.0 + 1000.0);

    int pwm_signal = 1500; // длительность импульса в микросекундах
    int duty = pwm_duty(pwm_signal); // 20 мс = 20000 мкс, 16384 = 2^14 (разрешение 14 бит)
    setting_pwm_pin(LED_PIN, PWM_FREQ, PWM_RES, 0); // частота 50 Гц, разрешение 14 бит, канал 0
    

    led_strip_handle_t led_strip = led_config();
    while (1) {
        led_on_chip(led_strip);

        pwm_pin(LED_PIN, duty); // установка скважности
    }
}
