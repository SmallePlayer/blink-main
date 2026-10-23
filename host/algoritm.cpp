#include "algoritm.h"
#include <cmath>
#include "core/net.hpp"

using namespace net;

static double normalize(double& variable) {
    if (variable >  180) variable -= 360;
    if (variable < -180) variable += 360;
    return variable;
}

struct Position {    // структура для хранения позиции и угла
    double x_s;
    double y_s;
    double angle_s;
    double x_t;
    double y_t;
};

struct Power {  // структура для отправки мощности на контроллер
    double left_power;
    double right_power;
};

Position input(int sock){       // функция получения данных 
    Position input_var;
    receive_struct(sock, &input_var);
    return input_var;
}

void output(int sock, Power power){     //Функция отправки данных
    send_struct(sock, &power);
}



int main(){
    Position var;

    
    int sock1 = create_socket((char*)"tcp");
    sockaddr_in addr1{};
    settings_client(sock1, &addr1, 8089);
    connect_to_server(sock1, &addr1);

    int sock2 = create_socket((char*)"tcp");
    sockaddr_in addr2{};
    settings_server(sock2, &addr2, 8090);
    bind_socket(sock2, &addr2);
    listen_socket(sock2);
    int client_sock = accept_connection(sock2, &addr2);
    


    double wall = 12.0;

    double left_power = 0; //левая мощность 
    double right_power = 0; // правая мощнсоть

    const double robot_length = 1.3; // длина робота в метрах
    const double robot_width = 0.7; // ширина робота в метрах
    
    const double diametr_weel = 0.255; // диаметр колеса
    const double radius = diametr_weel / 2; // радиус колеса в метрах

    const double track = 0.65; // ширина колесной базы
    const double RPM_weel = 600;

    const double power_max = 100.0; // максимальная мощность 
    const double v_max = RPM_weel * 2 * M_PI * radius / 60; // максимальная скорость


    // ПИД
    const double kp = 0.3;
    const double kd = 0.1;
    double error_angle_prev = 0;



    double target_left = 0, target_right = 0;

    double d_safe = 1.0; // безопасная дистанция до стены
    
    double kw = 3.0; // коэффициент силы отталкивания от стены


    auto prev = std::chrono::steady_clock::now();

    while (true){
        var = input(sock1);

        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - prev).count();
        prev = now;
        

        double rep_x = 0.0, rep_y = 0.0; 

        // Расчет вектора направления робота
        double dx = var.x_t - var.x_s;
        double dy = var.y_t - var.y_s;


        // Расчет силы отталкивания от стен
        if (var.x_s < d_safe) rep_x += (d_safe - var.x_s);
        if (wall - var.x_s < d_safe) rep_x -= (d_safe - (wall - var.x_s));
        if (var.y_s < d_safe) rep_y += (d_safe - var.y_s);
        if (wall - var.y_s < d_safe) rep_y -= (d_safe - (wall - var.y_s));

        // Расчет результирующего вектора с учетом силы отталкивания
        double gx = dx + kw * rep_x;
        double gy = dy + kw * rep_y;

        // Расчет угла поворота робота к цели с учетом силы отталкивания
        double desired = atan2(gy, gx) * 180 / M_PI;
        // Расчет ошибки угла поворота
        double error_angle = desired - var.angle_s;
        normalize(error_angle);
        
        // Расчет производной ошибки угла поворота
        double derivative = (error_angle - error_angle_prev) / dt;
        // Расчет корректирующего сигнала для ПИД-регулятора
        double correction = kp * error_angle + kd * derivative;

        // Расчет целевых скоростей для левого и правого колес с учетом корректирующего сигнала
        target_left = v_max - correction;
        target_right = v_max + correction;
        error_angle_prev = error_angle;

        // Ограничение целевых скоростей в пределах максимальной скорости
        if (target_left >  v_max) target_left =  v_max;
        if (target_left < -v_max) target_left = -v_max;
        if (target_right >  v_max) target_right =  v_max;
        if (target_right < -v_max) target_right = -v_max;

        // Расчет мощности для левого и правого колес в процентах от максимальной мощности
        left_power = (target_left  / v_max) * 100;
        right_power = (target_right / v_max) * 100;
        if (left_power >  power_max) left_power =  power_max;
        if (left_power < -power_max) left_power = -power_max;
        if (right_power >  power_max) right_power =  power_max;
        if (right_power < -power_max) right_power = -power_max;

        Power power;
        power.left_power = left_power;
        power.right_power = right_power;
        output(client_sock, power);
    }
}

