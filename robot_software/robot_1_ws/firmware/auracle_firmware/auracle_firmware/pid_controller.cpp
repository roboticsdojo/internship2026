// pid_controller.cpp
#include "pid_controller.h"

PIDController::PIDController(double kp, double ki, double kd, double out_min, double out_max)
    : _kp(kp), _ki(ki), _kd(kd), _out_min(out_min), _out_max(out_max) {}

double PIDController::compute(double target, double actual, double dt) {
    if (dt <= 0.0) {
        return 0.0;
    }

    double error = target - actual;

    // Tentative integral update, applied only if it doesn't push the output
    // past the clamp (basic anti-windup) - otherwise the integral term keeps
    // growing while the motor is already saturated and causes overshoot
    // once the error direction flips.
    double tentative_integral = _integral + error * dt;
    double derivative = (error - _prev_error) / dt;
    double output = (_kp * error) + (_ki * tentative_integral) + (_kd * derivative);

    if (output > _out_max) {
        output = _out_max;
    } else if (output < _out_min) {
        output = _out_min;
    } else {
        _integral = tentative_integral;
    }

    _prev_error = error;
    return output;
}

void PIDController::reset() {
    _integral = 0;
    _prev_error = 0;
}
