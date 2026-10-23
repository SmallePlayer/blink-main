#include "core/net.hpp"
#include <cmath>
#include <csignal>

using namespace net;

struct Power {  // структура, которую принимает ESP32
    double left_power;
    double right_power;
};

static volatile std::sig_atomic_t g_run = 1;

static void on_sigint(int) {
    g_run = 0;
}

int main() {
    std::signal(SIGINT, on_sigint);

    // сервер: ESP32 подключается к нам и получает Power
    int srv = create_socket((char*)"tcp");
    return_connection(srv);      // SO_REUSEADDR: переиспользовать порт после перезапуска
    sockaddr_in addr{};
    settings_server(srv, &addr, 8090);
    bind_socket(srv, &addr);
    listen_socket(srv);

    int cli = accept_connection(srv, &addr);
    std::cout << "ESP32 connected" << std::endl;

    double t = 0;
    while (g_run) {
        Power power;
        power.left_power  = 100.0 * std::sin(t);
        power.right_power = 100.0 * std::cos(t);
        send_struct(cli, &power);
        std::cout << "sent left=" << power.left_power
                  << " right=" << power.right_power << std::endl;
        t += 0.5;
        delay_milliseconds(20);  // пауза, чтобы не флудить ESP32
    }

    Power stop{0.0, 0.0};
    send_struct(cli, &stop);
    std::cout << "stopped: left=0 right=0" << std::endl;
    return 0;
}
