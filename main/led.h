#pragma once

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"



void led_white_one(int led_pin){
    gpio_reset_pin(led_pin);
    gpio_set_direction(led_pin, GPIO_MODE_OUTPUT);

    gpio_set_level(led_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_set_level(led_pin, 0);
}

void led_white_on(int led_pin){
    gpio_reset_pin(led_pin);
    gpio_set_direction(led_pin, GPIO_MODE_OUTPUT);

    gpio_set_level(led_pin, 1);
}

void led_white_off(int led_pin){
    gpio_reset_pin(led_pin);
    gpio_set_direction(led_pin, GPIO_MODE_OUTPUT);

    gpio_set_level(led_pin, 0);
}


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