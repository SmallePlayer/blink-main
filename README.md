# blink-main

Проект управления мобильным роботом на **ESP32-S3** (ESP-IDF): Wi-Fi, TCP/UDP-связь с компьютером, OLED-дисплей, RGB-подсветка, ШИМ для моторов и веб-интерфейс.

Сетевые функции (`net.c` / `net.h`) вынесены в отдельный репозиторий:
**https://github.com/SmallePlayer/net_esp**

---

## Структура

```
.
├── CMakeLists.txt          корневой CMake проекта ESP-IDF
├── main/                   основное приложение
│   ├── main.c              точка входа, короткая вспышка LED
│   ├── input_robot.c       автономная версия: Wi-Fi + TCP + моторы + RGB
│   ├── input_robot_lib.c   то же, но через библиотеку net.h
│   ├── net.c / net.h       сетевая библиотека (см. репозиторий net_esp)
│   ├── wifi_web.c          HTTP-сервер с веб-интерфейсом управления
│   ├── oled.c              драйвер OLED SSD1306 (I2C) и фреймбуфер
│   ├── oled_font.h         шрифт для OLED
│   ├── ppm.c               управление адресным RGB-светодиодом
│   ├── info.c              шпаргалка по GPIO/FreeRTOS (не компилируется)
│   ├── led.h               простые функции работы с GPIO-светодиодом
│   └── CMakeLists.txt      список исходников компонента main
├── host/                   код для компьютера (C++17)
│   ├── algoritm.cpp/.h     алгоритм движения/ПИД-регулятор
│   ├── sender.cpp          TCP-сервер, отправляет команды на ESP32
│   └── core/               сетевая библиотека для хоста (net.hpp/net.cpp)
├── test_server.py          Python-сервер для проверки связи
├── flash.sh                сборка + прошивка + монитор
├── dependencies.lock       lock-файл менеджера компонентов
├── sdkconfig*              конфигурация ESP-IDF (в .gitignore)
└── README.md
```

---

## Требования

- **ESP-IDF v6.1** (или совместимая версия);
- ESP32-S3;
- Python 3 — для `test_server.py`;
- CMake + C++17 — для сборки `host/`;
- компонент `espressif/led_strip` (подтягивается через `main/idf_component.yml`).

Перед сборкой активируйте окружение IDF:

```bash
source ~/.espressif/tools/activate_idf_v6.1.sh
```

---

## Сборка и прошивка

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodem101 flash monitor
```

Либо скриптом (сам находит USB-порт):

```bash
./flash.sh            # автоопределение порта
./flash.sh /dev/cu.usbmodem101
```

Выход из монитора — `Ctrl-]`.

### Получение net.c / net.h

Этих файлов нет в репозитории. Скопируйте их из **net_esp** в `main/`:

```bash
git clone https://github.com/SmallePlayer/net_esp
cp net_esp/net.c net_esp/net.h main/
```

---

## Основные компоненты

### `main/input_robot.c`

Автономный пример: подключается к Wi-Fi, как TCP-клиент принимает структуру из двух `double` (мощности левого и правого мотора), преобразует её в ШИМ (LEDC, 50 Гц, 14 бит) и управляет моторами. Дополнительно — адресный RGB-светодиод.

### `main/input_robot_lib.c`

То же самое, но с использованием `net.h`: `wifi_init_sta()` и `recv_all()` вместо самописного сетевого кода.

### `main/wifi_web.c`

HTTP-сервер на порту 80: отдаёт веб-страницу и принимает команды управления роботом через GET-запросы. Работает в режиме Wi-Fi-станции.

### `main/oled.c`

Драйвер OLED-дисплея SSD1306 128×64 по I2C (`SDA=17`, `SCL=18`, `RST=21`), кнопка на `GPIO0`, программный фреймбуфер и шрифт из `oled_font.h`.

### `main/ppm.c`

Инициализация адресного светодиода (WS2812) через RMT и функции установки цвета/HSV.

### `host/`

- `sender.cpp` — TCP-сервер на порту `8090`: ждёт подключения ESP32 и отправляет структуру `Power { double left; double right; }`.
- `algoritm.cpp` — расчёт управляющих команд по положению робота и цели (ПИД + отталкивание от стен).
- `core/` — обёртки сокетов для хоста.

Сборка:

```bash
cd host
cmake -S . -B build
cmake --build build
./build/sender
```

### `test_server.py`

Минимальный Python-сервер на `0.0.0.0:8090`, который шлёт ESP32 синусоидальные значения двух `double` (`struct.pack("<dd", left, right)`) — удобно для проверки связи без C++.

```bash
python3 test_server.py
```

---

## Сетевая конфигурация

IP-адрес сервера, порт, SSID и пароль задаются макросами в начале исходников:

```c
#define WIFI_SSID  "kaban2"
#define WIFI_PASS  "1234567890"
#define HOST_IP    "192.168.10.94"
#define HOST_PORT  8090
```

Поменяйте их под свою сеть. Компьютер и ESP32 должны быть в одной подсети, порт `8090` — открыт.

---

## Формат данных

ESP32 — little-endian. Python на хосте должен использовать little-endian формат:

```python
struct.pack("<dd", left, right)
```

---

## Замечания

- `info.c` содержит справочные комментарии и не участвует в сборке.
- `sdkconfig`, `sdkconfig.old`, `build/`, `managed_components/` исключены через `.gitignore`.
- `net.c`/`net.h` поддерживаются отдельно в **net_esp**; при изменении API обновляйте оба репозитория.
- Пароли Wi-Fi в исходниках приведены для примера — не используйте реальные в публичном коде.

---

## Лицензия

Лицензия не указана.
