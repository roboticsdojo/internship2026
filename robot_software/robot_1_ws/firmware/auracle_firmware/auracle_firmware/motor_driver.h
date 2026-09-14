// motor_driver.h
#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H
#include <Arduino.h>
#include "config.h"

class MotorDriver {
public:
    MotorDriver(int pwm, int in1, int in2);
    void setSpeed(double speed); // -255 to 255
private:
    int _pwm, _in1, _in2;
};
#endif

