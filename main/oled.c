#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_ssd1306.h"
#include "esp_lcd_io_i2c.h"
#include <stdbool.h>
#include "oled_font.h"
#include "led.h"

#define BUTTON_PIN 0

#define OLED_SDA   17
#define OLED_SCL   18
#define OLED_RST   21
#define VEXT       36
#define OLED_ADDR  0x3C
#define OLED_W     128
#define OLED_H     64

// общие хендлы — видны всем функциям ниже
static i2c_master_bus_handle_t    g_i2c_bus = NULL;
static esp_lcd_panel_io_handle_t  g_io      = NULL;
static esp_lcd_panel_handle_t     g_panel   = NULL;

static uint8_t g_fb[OLED_W * OLED_H / 8];   // глобальный фреймбуфер


void oled_update()
{
    esp_lcd_panel_draw_bitmap(g_panel, 0, 0, OLED_W, OLED_H, g_fb);
}

void oled_set_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
    int idx = (y / 8) * 128 + x;
    if (on) g_fb[idx] |=  (1 << (y % 8));
    else    g_fb[idx] &= ~(1 << (y % 8));
}

void oled_fill(uint8_t value)
{
    uint8_t buf[OLED_W * OLED_H / 8];
    memset(buf, value, sizeof(buf));
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(g_panel, 0, 0, OLED_W, OLED_H, buf));
}

void oled_draw_char(int x, int y, char c)
{
    const uint8_t *g = font_glyph(c);
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (g[row] & (0x80 >> col)) {      // бит 7 = левый пиксель
                oled_set_pixel(x + col, y + row, 1);
            }
        }
    }
}

void oled_draw_string(int x, int y, const char *str)
{
    while (*str) {
        oled_draw_char(x, y, *str);
        x += 8;              // сдвиг на 8 пикселей вправо
        str++;
    }
}

void oled_power_on(void)
{
    gpio_reset_pin(VEXT);
    gpio_set_direction(VEXT, GPIO_MODE_OUTPUT);
    gpio_set_level(VEXT, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
}

void oled_i2c_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = OLED_SCL,
        .sda_io_num = OLED_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &g_i2c_bus));

    esp_lcd_panel_io_i2c_config_t io_cfg = {
        .dev_addr = OLED_ADDR,
        .scl_speed_hz = 400 * 1000,
        .transaction_timeout_ms = 1000,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(g_i2c_bus, &io_cfg, &g_io));
}

void oled_panel_init(void)
{
    esp_lcd_panel_ssd1306_config_t ssd_cfg = {
        .height = OLED_H,
    };
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = OLED_RST,
        .bits_per_pixel = 1,
        .vendor_config = &ssd_cfg,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(g_io, &panel_cfg, &g_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(g_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(g_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(g_panel, true));
}

void oled_clear(){
    memset(g_fb, 0, sizeof(g_fb));   // очистить глобальный буфер
    oled_update();
}

int state = 0;
int64_t last_press = 0;

int prev_btn = 1;

bool button_clicked(int BUT_PIN)
{
    static int prev = 1;                    // хранится между вызовами
    int cur = gpio_get_level(BUT_PIN);

    bool clicked = (prev == 1 && cur == 0); // фронт нажатия
    prev = cur;                             // запомнить на следующий вызов
    return clicked;
}

bool button_pressed(int BUT_PIN)
{
    return gpio_get_level(BUT_PIN) == 0; // 0 = нажата
}

void app_main(void)
{
    gpio_reset_pin(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);  

    oled_power_on();
    oled_i2c_init();
    oled_panel_init();

    oled_fill(0xFF);  
    vTaskDelay(pdMS_TO_TICKS(1000));
    memset(g_fb, 0, sizeof(g_fb));
    oled_update();
 

    static int prev_click = 0;
    bool click = 0;
    while(1){
        memset(g_fb, 0, sizeof(g_fb));
        
        int state = button_clicked(BUTTON_PIN);
        oled_draw_char(0, 0, '0' + state);        // вывести '0' или '1'
        int state_p = button_pressed(BUTTON_PIN);
        oled_draw_char(10, 0, '0' + state_p);        // вывести '0' или '1'

        
        if (state){
            click = !click;
        }
        if (click){
            led_white_on();
        }
        else{
            led_white_off();
        }
        
        oled_update();
       
        //oled_clear();
    }
}