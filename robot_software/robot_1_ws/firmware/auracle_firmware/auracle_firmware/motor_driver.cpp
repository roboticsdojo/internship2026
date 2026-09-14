// motor_driver.cpp
#include "motor_driver.h"
#include <math.h>

MotorDriver::MotorDriver(int pwm, int in1, int in2) : _pwm(pwm), _in1(in1), _in2(in2) {
    pinMode(_pwm, OUTPUT); pinMode(_in1, OUTPUT); pinMode(_in2, OUTPUT);
}
void MotorDriver::setSpeed(double speed) {
    // Arduino's abs() is an int macro - on a double it silently truncates
    // to int before taking the absolute value, which is wrong for anything
    // between -1.0 and 1.0. fabs() is the correct floating-point call.
    int pwmVal = (int)constrain(fabs(speed), 0, 255);
    digitalWrite(_in1, speed > 0 ? HIGH : LOW);
    digitalWrite(_in2, speed > 0 ? LOW : HIGH);
    analogWrite(_pwm, pwmVal);
}
