//source /Users/pes/.espressif/tools/activate_idf_v6.1.sh
//idf.py -p /dev/cu.usbmodem101 flash monitor
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define led_pin 35

void led_white(){
    gpio_reset_pin(led_pin);
    gpio_set_direction(led_pin, GPIO_MODE_OUTPUT);

    gpio_set_level(led_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(led_pin, 0);
}



void app_main(void){
    led_white();
    
}