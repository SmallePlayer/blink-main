#pragma once

struct RobotState {
    double x;
    double y;
    double angle;   // градусы
};

struct Target {
    double x;
    double y;
};

struct Cmd {
    double left;    // -100..100
    double right;   // -100..100
};

struct Params {
    double v_max;   // м/с при команде 100
    double track;   // колея, м
    double arena;   // размер арены, м
    double radius;  // радиус робота, м
    double d_safe;  // зазор до стены, м
    double kw;      // сила отталкивания
    double kp;      // ПИД: пропорциональный
    double kd;      // ПИД: дифференциальный
    double base;    // базовый газ, обычно 100
};

class Controller {
public:
    explicit Controller(const Params& p);
    Cmd update(const RobotState& self, const Target& target, double dt);

private:
    Params p;
    double error_prev;
};
