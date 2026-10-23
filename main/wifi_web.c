//source /Users/pes/.espressif/tools/activate_idf_v6.1.sh
//idf.py -p /dev/cu.usbmodem101 flash monitor

#include <string.h>          // работа со строками и памятью
#include <unistd.h>          // close() для сокета
#include <stdlib.h>          // malloc(), atoi()
#include <errno.h>           // errno / EAGAIN
#include <sys/socket.h>      // socket(), bind(), listen(), accept(), recv(), send()
#include <sys/time.h>        // struct timeval (таймаут сокета)
#include <arpa/inet.h>       // htons(), inet_ntoa(), sockaddr_in
#include "freertos/FreeRTOS.h"   // базовые типы FreeRTOS
#include "freertos/task.h"       // vTaskDelay (паузы задачи)
#include "esp_wifi.h"            // функции драйвера WiFi
#include "esp_event.h"           // система событий (подключение/отключение)
#include "esp_log.h"             // ESP_LOGI (логирование)
#include "nvs_flash.h"           // инициализация NVS-хранилища
#include "esp_netif.h"           // сетевой стек (TCP/IP поверх WiFi)
#include "esp_netif_ip_addr.h"   // макросы IPSTR / IP2STR
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "driver/ledc.h"
#include "led_strip.h"

#define WIFI_SSID  "TP-Link_44E6"      // имя Wi-Fi сети (2.4 ГГц)
#define WIFI_PASS  "49887037"          // пароль
#define HTTP_PORT  80                  // порт веб-сервера (стандартный)

#define LED_GPIO 21

#define PWM_FREQ 50      // частота ШИМ, Гц
#define PWM_RES 14      // разрешение ШИМ, бит (14 бит = 0..16383)
#define LEFT_MOTOR_PIN 10
#define RIGHT_MOTOR_PIN 9
#define ESC_PIN 11                          // регулятор хода (ползунок 0..100)
#define ESC_CHANNEL LEDC_CHANNEL_2

// магический ключ WebSocket (добавляется к Sec-WebSocket-Key перед SHA-1)
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

static const char *TAG = "robot";      // метка для логов

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

// зажечь светодиод заданным цветом (0..255 на канал)
static void set_led(led_strip_handle_t led, uint8_t r, uint8_t g, uint8_t b){
    led_strip_set_pixel(led, 0, r, g, b);
    led_strip_refresh(led);
}

// обработчик событий WiFi
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    // при старте и при отключении — подключаемся заново
    if (event_base == WIFI_EVENT &&
        (event_id == WIFI_EVENT_STA_START || event_id == WIFI_EVENT_STA_DISCONNECTED))
        esp_wifi_connect();
}

// обработчик получения IP-адреса — печатаем, какой URL открывать в браузере
static void got_ip_handler(void* arg, esp_event_base_t event_base,
                           int32_t event_id, void* event_data)
{
    ip_event_got_ip_t *evt = (ip_event_got_ip_t*)event_data;
    ESP_LOGI(TAG, "WiFi connected, IP: " IPSTR, IP2STR(&evt->ip_info.ip));
    ESP_LOGI(TAG, "Open http://" IPSTR " in browser", IP2STR(&evt->ip_info.ip));
}

// инициализация WiFi в режиме станции (клиента)
void wifi_init_sta(void)
{
    esp_netif_init();                       // инициализация сетевого стека
    esp_event_loop_create_default();        // цикл обработки событий
    esp_netif_create_default_wifi_sta();    // интерфейс "станция"

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();  // конфиг по умолчанию
    esp_wifi_init(&cfg);                    // инициализация драйвера WiFi

    // регистрируем обработчики событий WiFi и получения IP
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_handler, NULL);

    wifi_config_t wifi_config = {
        .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);       // режим клиента
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);  // задаём SSID и пароль
    esp_wifi_start();                       // запускаем WiFi
    esp_wifi_set_ps(WIFI_PS_NONE);          // отключаем энергосбережение -> меньше задержки
}

// принимает ровно len байт (сокет может вернуть меньше за один вызов)
// возвращает total, -1 при ошибке/закрытии, -2 при таймауте в начале кадра
static int recv_all(int sock, void *buf, int len)
{
    int total = 0;              // сколько байт уже принято
    char *p = (char *)buf;      // буфер как массив байт
    while (total < len) {       // пока не набрали нужное количество
        int n = recv(sock, p + total, len - total, 0);
        if (n == 0) return -1;  // клиент закрыл соединение
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return total == 0 ? -2 : -1;  // таймаут ожидания данных
            return -1;          // ошибка
        }
        total += n;
    }
    return total;
}

// отправляет ровно len байт (send может отправить меньше)
static int send_all(int sock, const void *buf, int len)
{
    const char *p = (const char *)buf;
    int total = 0;
    while (total < len) {
        int n = send(sock, p + total, len - total, 0);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

// power (-100..100) -> длительность импульса в мкс (1000..2000)
int output_power(double power)
{
    return (int)round((power + 100.0) * 5.0 + 1000.0);
}

// настройка ШИМ-канала сервы
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

static ledc_channel_t channel_for_pin(int pin){
    if (pin == LEFT_MOTOR_PIN)  return LEDC_CHANNEL_0;
    if (pin == RIGHT_MOTOR_PIN) return LEDC_CHANNEL_1;
    return ESC_CHANNEL;
}

void pwm_pin(int pin, int variable){
    ledc_channel_t ch = channel_for_pin(pin);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, variable);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

int pwm_duty(int pwm_signal);

// ползунок 0..100 -> импульс 1000..2000 мкс для регулятора хода
static void esc_set(int percent){
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;
    pwm_pin(ESC_PIN, pwm_duty(1000 + percent * 10));
}

int pwm_duty(int pwm_signal){
    return pwm_signal * (1 << PWM_RES) / (1000000 / PWM_FREQ); // 20 мс = 20000 мкс, 16384 = 2^14
}

// танковый привод: y = газ (-1..1, + вперёд), x = поворот (-1..1, + вправо)
static void apply_joystick(float x, float y)
{
    // ограничиваем значения в диапазоне [-1, 1]
    if (x < -1.0f) x = -1.0f;
    if (x >  1.0f) x =  1.0f;
    if (y < -1.0f) y = -1.0f;
    if (y >  1.0f) y =  1.0f;

    float left  = y + x;    // смешиваем газ и поворот
    float right = y - x;

    // если суммарная мощность превышает 100% — пропорционально уменьшаем
    float m = fmaxf(fabsf(left), fabsf(right));
    if (m > 1.0f) { left /= m; right /= m; }

    int left_us  = output_power(left  * 100.0);
    int right_us = output_power(right * 100.0);

    pwm_pin(LEFT_MOTOR_PIN,  pwm_duty(left_us));
    pwm_pin(RIGHT_MOTOR_PIN, pwm_duty(right_us));
}

// ===================== WebSocket =====================

static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// циклический сдвиг влево (нужен для SHA-1)
static uint32_t rotl32(uint32_t v, int n)
{
    return (v << n) | (v >> (32 - n));
}

// SHA-1 (собственная реализация, без mbedtls). Вход — короткая строка (key+GUID)
static void sha1_compute(const uint8_t *msg, size_t len, uint8_t out[20])
{
    uint32_t h0 = 0x67452301u, h1 = 0xEFCDAB89u, h2 = 0x98BADCFEu,
             h3 = 0x10325476u, h4 = 0xC3D2E1F0u;

    uint64_t bitlen = (uint64_t)len * 8;
    size_t padded = ((len + 8) / 64 + 1) * 64;   // место под 0x80 + 8 байт длины
    uint8_t buf[128];                            // достаточно для key+GUID (~60 байт)

    memset(buf, 0, sizeof(buf));
    memcpy(buf, msg, len);
    buf[len] = 0x80;                             // маркер конца сообщения
    for (int i = 0; i < 8; i++)                  // длина в big-endian в конце
        buf[padded - 1 - i] = (uint8_t)((bitlen >> (8 * i)) & 0xFF);

    for (size_t off = 0; off < padded; off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)buf[off + i*4]     << 24)
                 | ((uint32_t)buf[off + i*4 + 1] << 16)
                 | ((uint32_t)buf[off + i*4 + 2] << 8)
                 | ((uint32_t)buf[off + i*4 + 3]);
        for (int i = 16; i < 80; i++)
            w[i] = rotl32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | ((~b) & d);   k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d;              k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
            else             { f = b ^ c ^ d;              k = 0xCA62C1D6u; }
            uint32_t t = rotl32(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotl32(b, 30); b = a; a = t;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    out[0]  = (h0 >> 24) & 0xFF; out[1]  = (h0 >> 16) & 0xFF; out[2]  = (h0 >> 8) & 0xFF; out[3]  = h0 & 0xFF;
    out[4]  = (h1 >> 24) & 0xFF; out[5]  = (h1 >> 16) & 0xFF; out[6]  = (h1 >> 8) & 0xFF; out[7]  = h1 & 0xFF;
    out[8]  = (h2 >> 24) & 0xFF; out[9]  = (h2 >> 16) & 0xFF; out[10] = (h2 >> 8) & 0xFF; out[11] = h2 & 0xFF;
    out[12] = (h3 >> 24) & 0xFF; out[13] = (h3 >> 16) & 0xFF; out[14] = (h3 >> 8) & 0xFF; out[15] = h3 & 0xFF;
    out[16] = (h4 >> 24) & 0xFF; out[17] = (h4 >> 16) & 0xFF; out[18] = (h4 >> 8) & 0xFF; out[19] = h4 & 0xFF;
}

// кодирование в Base64 (нужно для Sec-WebSocket-Accept)
static int base64_encode(const unsigned char *src, int slen, char *dst, int dlen)
{
    int olen = 0;
    for (int i = 0; i < slen; i += 3) {
        unsigned a = src[i];
        unsigned b = (i + 1 < slen) ? src[i + 1] : 0;
        unsigned c = (i + 2 < slen) ? src[i + 2] : 0;
        unsigned triple = (a << 16) | (b << 8) | c;
        if (olen + 4 >= dlen) break;
        dst[olen++] = b64_chars[(triple >> 18) & 0x3F];
        dst[olen++] = b64_chars[(triple >> 12) & 0x3F];
        dst[olen++] = (i + 1 < slen) ? b64_chars[(triple >> 6) & 0x3F] : '=';
        dst[olen++] = (i + 2 < slen) ? b64_chars[triple & 0x3F] : '=';
    }
    dst[olen] = '\0';
    return olen;
}

// читает HTTP-заголовки запроса (до пустой строки \r\n\r\n)
static int read_http_headers(int sock, char *buf, int max)
{
    int total = 0;
    while (total < max - 1) {
        int n = recv(sock, buf + total, max - 1 - total, 0);
        if (n <= 0) return -1;
        total += n;
        buf[total] = '\0';
        if (strstr(buf, "\r\n\r\n")) return total;
    }
    return -1;
}

// достаёт значение заголовка по имени (напр. "Sec-WebSocket-Key")
static char *http_header(char *buf, const char *name, char *out, int outlen)
{
    char *p = strstr(buf, name);
    if (!p) return NULL;
    p += strlen(name);
    while (*p == ' ' || *p == ':') p++;
    char *end = strstr(p, "\r\n");
    int len = end ? (int)(end - p) : (int)strlen(p);
    if (len >= outlen) len = outlen - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return out;
}

// ответ на WebSocket-рукопожатие (101 Switching Protocols)
static void ws_handshake_reply(int sock, const char *key)
{
    unsigned char sha[20];
    char concat[256];
    snprintf(concat, sizeof(concat), "%s%s", key, WS_GUID);
    sha1_compute((const unsigned char*)concat, strlen(concat), sha);

    char b64[64];
    base64_encode(sha, 20, b64, sizeof(b64));

    char resp[256];
    snprintf(resp, sizeof(resp),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n", b64);
    send_all(sock, resp, strlen(resp));
}

// отправка WebSocket-кадра от сервера (без маски)
static int ws_send(int sock, int opcode, const void *data, int len)
{
    unsigned char hdr[10];
    int hlen = 0;
    hdr[0] = 0x80 | opcode;              // FIN + opcode
    if (len < 126) {
        hdr[1] = len;
        hlen = 2;
    } else if (len < 65536) {
        hdr[1] = 126;
        hdr[2] = (len >> 8) & 0xFF;
        hdr[3] = len & 0xFF;
        hlen = 4;
    } else {
        hdr[1] = 127;
        uint64_t l = (uint64_t)len;
        for (int i = 8; i >= 1; i--) { hdr[i] = (unsigned char)(l & 0xFF); l >>= 8; }
        hlen = 10;
    }
    if (send_all(sock, hdr, hlen) < 0) return -1;
    if (len && send_all(sock, data, len) < 0) return -1;
    return 0;
}

// приём одного WebSocket-кадра: пишет текст в payload, возвращает opcode
static int ws_recv(int sock, char *payload, int max, int *opcode_out)
{
    unsigned char hdr[2];
    int r = recv_all(sock, hdr, 2);
    if (r == -2) return -2;                 // таймаут — вызывающий шлёт ping
    if (r < 0) return -1;
    int opcode = hdr[0] & 0x0F;
    int masked = (hdr[1] >> 7) & 1;
    uint64_t len = hdr[1] & 0x7F;

    if (len == 126) {
        unsigned char ext[2];
        if (recv_all(sock, ext, 2) < 0) return -1;
        len = (ext[0] << 8) | ext[1];
    } else if (len == 127) {
        unsigned char ext[8];
        if (recv_all(sock, ext, 8) < 0) return -1;
        len = 0;
        for (int i = 0; i < 8; i++) len = (len << 8) | ext[i];
    }

    unsigned char mask[4] = {0};
    if (masked && recv_all(sock, mask, 4) < 0) return -1;

    if (len > (uint64_t)(max - 1)) len = max - 1;
    if (len && recv_all(sock, payload, (int)len) < 0) return -1;
    payload[len] = '\0';

    if (masked)
        for (uint64_t i = 0; i < len; i++) payload[i] ^= mask[i & 3];

    *opcode_out = opcode;
    return (int)len;
}

// главный цикл WebSocket-соединения: принимаем координаты джойстика
static void ws_loop(int sock, led_strip_handle_t led)
{
    char payload[128];
    while (1) {
        int opcode = 0;
        int n = ws_recv(sock, payload, sizeof(payload), &opcode);
        if (n == -2) {                          // клиент молчит — проверяем, жив ли
            if (ws_send(sock, 0xA, NULL, 0) < 0) break;
            continue;
        }
        if (n < 0) break;                       // соединение оборвалось
        if (opcode == 0x8) break;               // close — клиент закрыл
        if (opcode == 0x9) {                    // ping -> pong
            ws_send(sock, 0xA, payload, n);
            continue;
        }
        if (opcode != 0x1 && opcode != 0x2) continue;  // только текстовые/бинарные данные

        // ползунок: сообщение вида "S:0..100"
        if (payload[0] == 'S' && payload[1] == ':') {
            esc_set(atoi(payload + 2));
            continue;
        }

        float x = 0.0f, y = 0.0f;
        if (sscanf(payload, "%f,%f", &x, &y) == 2)
            apply_joystick(x, y);
    }
}

// ===================== HTTP-страница =====================

// страница с виртуальным джойстиком (все кавычки одинарные, чтобы не экранировать)
static const char index_html[] =
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"<meta charset='utf-8'>\n"
"<meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'>\n"
"<title>Robot</title>\n"
"<style>\n"
"html,body{margin:0;height:100%;background:#111;color:#eee;font-family:sans-serif;touch-action:none;overflow:hidden}\n"
"#wrap{display:flex;flex-direction:column;height:100%;align-items:center;justify-content:center}\n"
"#status{margin-bottom:12px;font-size:15px;color:#0f0}\n"
"#zone{position:relative;width:min(88vw,88vh);height:min(88vw,88vh)}\n"
"#base{position:absolute;inset:0;border-radius:50%;background:#1e1e1e;border:3px solid #444}\n"
"#knob{position:absolute;width:88px;height:88px;border-radius:50%;background:#0af;left:50%;top:50%;transform:translate(-50%,-50%);pointer-events:none}\n"
"#readout{margin-top:14px;font-size:14px;color:#aaa}\n"
"#thrbox{margin-top:18px;display:flex;align-items:center;gap:12px;font-size:14px;color:#aaa}\n"
"#thr{width:min(70vw,320px);touch-action:auto}\n"
"#thrval{min-width:34px;text-align:right;color:#0af}\n"
"</style>\n"
"</head>\n"
"<body>\n"
"<div id='wrap'>\n"
"<div id='status'>connecting...</div>\n"
"<div id='zone'><div id='base'></div><div id='knob'></div></div>\n"
"<div id='readout'>L:0 R:0</div>\n"
"<div id='thrbox'><span>ESC</span><input id='thr' type='range' min='0' max='100' value='0'><span id='thrval'>0</span></div>\n"
"</div>\n"
"<script>\n"
"const zone=document.getElementById('zone');\n"
"const knob=document.getElementById('knob');\n"
"const status=document.getElementById('status');\n"
"const readout=document.getElementById('readout');\n"
"const thr=document.getElementById('thr');\n"
"const thrval=document.getElementById('thrval');\n"
"let ws;\n"
"function connect(){\n"
"ws=new WebSocket('ws://'+location.host+'/ws');\n"
"ws.onopen=()=>{status.textContent='connected';if(ws.readyState===1)ws.send('S:'+thr.value);};\n"
"ws.onclose=()=>{status.textContent='disconnected';setTimeout(connect,1000);};\n"
"}\n"
"connect();\n"
"thr.addEventListener('input',()=>{thrval.textContent=thr.value;if(ws&&ws.readyState===1)ws.send('S:'+thr.value);});\n"
"let active=false;\n"
"function send(x,y){\n"
"if(ws.readyState===1)ws.send(x.toFixed(3)+','+y.toFixed(3));\n"
"const L=Math.max(-1,Math.min(1,y+x));\n"
"const R=Math.max(-1,Math.min(1,y-x));\n"
"readout.textContent='L:'+Math.round(L*100)+' R:'+Math.round(R*100);\n"
"}\n"
"function handle(e){\n"
"const r=zone.getBoundingClientRect();\n"
"const cx=r.left+r.width/2,cy=r.top+r.height/2;\n"
"let x=(e.clientX-cx)/(r.width/2);\n"
"let y=(cy-e.clientY)/(r.height/2);\n"
"const len=Math.hypot(x,y);\n"
"if(len>1){x/=len;y/=len;}\n"
"knob.style.transform='translate(-50%,-50%) translate('+x*(r.width/2-44)+'px,'+(-y)*(r.height/2-44)+'px)';\n"
"send(x,y);\n"
"}\n"
"zone.addEventListener('pointerdown',e=>{active=true;zone.setPointerCapture(e.pointerId);handle(e);});\n"
"zone.addEventListener('pointermove',e=>{if(active)handle(e);});\n"
"zone.addEventListener('pointerup',()=>{active=false;send(0,0);});\n"
"zone.addEventListener('pointercancel',()=>{active=false;send(0,0);});\n"
"</script>\n"
"</body>\n"
"</html>\n";

// отправить страницу как HTTP-ответ 200
static void http_send_page(int sock)
{
    char hdr[256];
    snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n", (int)strlen(index_html));
    send_all(sock, hdr, strlen(hdr));
    send_all(sock, index_html, strlen(index_html));
}

// один клиент обслуживается в отдельной задаче, чтобы медленный/зависший
// WebSocket или «тихое» preconnect-соединение браузера не блокировали остальных
typedef struct {
    int sock;
    led_strip_handle_t led;
} client_ctx_t;

static void client_task(void *arg)
{
    client_ctx_t *ctx = (client_ctx_t *)arg;
    int cli = ctx->sock;
    led_strip_handle_t led = ctx->led;
    free(ctx);

    // таймаут приёма: незанятые соединения не висят вечно
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(cli, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int ka = 1;
    setsockopt(cli, SOL_SOCKET, SO_KEEPALIVE, &ka, sizeof(ka));

    char req[1024];
    if (read_http_headers(cli, req, sizeof(req)) < 0) {
        close(cli);
        vTaskDelete(NULL);
        return;
    }

    char key[128];
    if (http_header(req, "Sec-WebSocket-Key", key, sizeof(key))) {
        // это WebSocket-клиент: рукопожатие и непрерывный поток координат
        ws_handshake_reply(cli, key);
        set_led(led, 0, 255, 0);   // зелёный — управление активно
        ws_loop(cli, led);
        set_led(led, 0, 0, 0);     // выключаем
        apply_joystick(0, 0);      // стоп при отключении
        esc_set(0);                // газ в ноль
    } else {
        // обычный запрос браузера — отдаём страницу
        http_send_page(cli);
    }
    close(cli);
    vTaskDelete(NULL);
}

void app_main(void)
{
    nvs_flash_init();       // нужно драйверу WiFi
    wifi_init_sta();        // поднимаем WiFi

    // настройка ШИМ моторов и регулятора хода
    setting_pwm_pin(LEFT_MOTOR_PIN, PWM_FREQ, PWM_RES, 0);
    setting_pwm_pin(RIGHT_MOTOR_PIN, PWM_FREQ, PWM_RES, 1);
    setting_pwm_pin(ESC_PIN, PWM_FREQ, PWM_RES, ESC_CHANNEL);
    apply_joystick(0, 0);   // нейтраль (моторы стоят)
    esc_set(0);             // газ в ноль

    led_strip_handle_t led_strip = led_config();   // инициализация светодиода
    set_led(led_strip, 0, 0, 0);

    // создаём слушающий TCP-сокет на порту 80
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(HTTP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    bind(srv, (struct sockaddr *)&addr, sizeof(addr));
    listen(srv, 8);

    while (1) {              // бесконечный цикл: принимаем клиентов
        int cli = accept(srv, NULL, NULL);
        if (cli < 0) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }

        client_ctx_t *ctx = malloc(sizeof(client_ctx_t));
        if (!ctx) { close(cli); continue; }
        ctx->sock = cli;
        ctx->led  = led_strip;

        if (xTaskCreate(client_task, "client", 8192, ctx, 5, NULL) != pdPASS) {
            free(ctx);
            close(cli);
        }
    }
}
