//source /Users/pes/.espressif/tools/activate_idf_v6.1.sh
//idf.py -p /dev/cu.usbmodem101 flash monitor

#include <unistd.h>          // close() для сокета
#include <sys/socket.h>      // sockaddr_in
#include "freertos/FreeRTOS.h"   // базовые типы FreeRTOS
#include "freertos/task.h"       // vTaskDelay (паузы задачи)
#include "esp_log.h"             // ESP_LOGI (логирование)
#include <math.h>
#include <stdbool.h>
#include "driver/ledc.h"
#include "led_strip.h"

#include "net.h"                // мини-библиотека WiFi/сокетов

#define WIFI_SSID  "kaban2"      // имя Wi-Fi сети (2.4 ГГц)
#define WIFI_PASS  "1234567890"  // пароль

#define LED_GPIO 21

#define PWM_FREQ 50      // частота ШИМ, Гц
#define PWM_RES 14      // разрешение ШИМ, бит (14 бит = 0..16383)
#define LEFT_MOTOR_PIN 11
#define RIGHT_MOTOR_PIN 9


static const char *TAG = "robot";      // метка для логов

// структура, которую присылает хост: мощность двух моторов
typedef struct {
    double left_power;
    double right_power;
} Power;


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

int output_power(double power)
{
    return (int)round((power + 100.0) * 5.0 + 1000.0);
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
    ledc_channel_t ch = (pin == LEFT_MOTOR_PIN) ? LEDC_CHANNEL_0 : LEDC_CHANNEL_1;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, variable);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

int pwm_duty(int pwm_signal){
    return pwm_signal * (1 << PWM_RES) / (1000000 / PWM_FREQ); // 20 мс = 20000 мкс, 16384 = 2^14 (разрешение 14 бит)
}

void app_main(void)
{
    wifi_init_sta(WIFI_SSID, WIFI_PASS);   // поднимаем WiFi (NVS инициализируется внутри библиотеки)
    setting_pwm_pin(LEFT_MOTOR_PIN, PWM_FREQ, PWM_RES, 0);
    setting_pwm_pin(RIGHT_MOTOR_PIN, PWM_FREQ, PWM_RES, 1);

    led_strip_handle_t led_strip = led_config();   // инициализация светодиода
    bool led_on = false;

    while (1) {              // бесконечный цикл: подключаемся и принимаем данные
        // библиотека сама создаёт сокет, заполняет адрес (HOST_IP/HOST_PORT) и переподключается
        int sock = connect_socket_tcp(HOST_IP, HOST_PORT);

        Power power;
        // принимаем структуру, пока соединение живо
        while (recv_all(sock, &power, sizeof(Power)) >= 0)
        {
            int left  = output_power(power.left_power);
            int right = output_power(power.right_power);
            int left_duty  = pwm_duty(left);
            int right_duty = pwm_duty(right);

            pwm_pin(LEFT_MOTOR_PIN, left_duty);
            pwm_pin(RIGHT_MOTOR_PIN, right_duty);
            vTaskDelay(1);   // даём планировщику подышать (не влияет на скорость ШИМ)

            // мигаем белым с частотой приёма: вкл/выкл на каждый пакет
            if (led_on) {
                led_strip_set_pixel(led_strip, 0, 255, 255, 255);  // белый
            } else {
                led_strip_set_pixel(led_strip, 0, 0, 0, 0);        // выключен
            }
            led_strip_refresh(led_strip);
            led_on = !led_on;
        }

        close(sock);          // соединение оборвалось — закрываем сокет
    }
}
