// pid_controller.h
#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H
class PIDController {
public:
    PIDController(double kp, double ki, double kd, double out_min = -255.0, double out_max = 255.0);
    double compute(double target, double actual, double dt);
    void reset();
private:
    double _kp, _ki, _kd;
    double _out_min, _out_max;
    double _integral = 0, _prev_error = 0;
};
#endif
