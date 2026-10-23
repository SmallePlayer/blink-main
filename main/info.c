
// Включить / выключить пин
// #define LED_PIN 2

// gpio_set_level(LED_PIN, 1);   // включить (HIGH, 3.3V)
// gpio_set_level(LED_PIN, 0);   // выключить (LOW, 0V)


// Задержка
// vTaskDelay(pdMS_TO_TICKS(1000));   // 1000 мс = 1 сек (FreeRTOS, не блокирует другие задачи)
// vTaskDelay(pdMS_TO_TICKS(1));      // 1 мс
// vTaskDelay(1000 / portTICK_PERIOD_MS); // тоже 1 сек (старый стиль)

// #include "esp_rom_gpio.h"
// esp_rom_delay_us(1000000);         // блокирующая задержка 1 сек (микросекунды)



// Настройка пина (все виды)

// Вариант 1 — простая установка направления:
// gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);   // выход
// gpio_set_direction(LED_PIN, GPIO_MODE_INPUT);    // вход
// Режимы: GPIO_MODE_OUTPUT, GPIO_MODE_INPUT, GPIO_MODE_OUTPUT_OD (open-drain), GPIO_MODE_INPUT_OUTPUT, GPIO_MODE_DISABLE.


// Вариант 2 — полная настройка через gpio_config():
// gpio_config_t io = {
//     .pin_bit_mask  = (1ULL << LED_PIN),   // какие пины (битовая маска)
//     .mode          = GPIO_MODE_OUTPUT,    // режим
//     .pull_up_en    = 0,                   // подтяжка вверх (0/1)
//     .pull_down_en  = 0,                   // подтяжка вниз (0/1)
//     .intr_type     = GPIO_INTR_DISABLE,   // прерывание: DISABLE/POSEDGE/NEGEDGE/ANYEDGE/LOW_LEVEL/HIGH_LEVEL
// };
// gpio_config(&io);

// Подтяжки отдельно:
// gpio_set_pull_mode(LED_PIN, GPIO_PULLUP_ONLY);   // GPIO_PULLUP_ONLY / GPIO_PULLDOWN_ONLY / GPIO_PULLUP_PULLDOWN / GPIO_FLOATING

// Сила тока (drive strength):
// gpio_set_drive_capability(LED_PIN, GPIO_DRIVE_CAP_3);  // CAP_0..CAP_3 (слабее → сильнее)

// Сброс пина
// gpio_reset_pin(LED_PIN);   // вернуть пин к состоянию по умолчанию


// #include "driver/ledc.h"

// #define PWM_PIN  2
// #define PWM_FREQ 5000   // частота, Гц
// #define PWM_RES  8      // разрешение, бит (8 бит = 0..255)

// // 1) таймер
// ledc_timer_config_t timer = {
//     .speed_mode      = LEDC_LOW_SPEED_MODE,  // режим скорости
//     .timer_num       = LEDC_TIMER_0,         // какой таймер (0..3)
//     .duty_resolution = LEDC_TIMER_8_BIT,     // разрешение
//     .freq_hz         = PWM_FREQ,             // частота
//     .clk_cfg         = LEDC_AUTO_CLK,        // источник тактов
// };
// ledc_timer_config(&timer);

// // 2) канал
// ledc_channel_config_t ch = {
//     .speed_mode = LEDC_LOW_SPEED_MODE,
//     .channel    = LEDC_CHANNEL_0,            // канал 0..7
//     .timer_sel  = LEDC_TIMER_0,
//     .gpio_num   = PWM_PIN,
//     .duty       = 0,                         // начальная скважность
//     .hpoint     = 0,
// };
// ledc_channel_config(&ch);

// Установить яркость (скважность):
// ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 128);  // 128/255 ≈ 50%
// ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);    // применить

// Изменить частоту на лету:
// ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, 1000);   // 1 кГц

// Плавное изменение (fade):
// ledc_set_fade_time_and_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 255, 1000, LEDC_FADE_NO_WAIT);
// // плавно дойти до 255 за 1000 мс