#include "pid_controller.h"
#include <math.h>

PIDController leftPID;
PIDController rightPID;

PIDController::PIDController()
{
    kp = 1.0;
    ki = 0.0;
    kd = 0.0;
    target = 0;
    integral = 0;
    previousError = 0;
    outputLimit = MOTOR_MAX_PWM;
}

void PIDController::setTarget(float value)
{
    target = value;
}

float PIDController::update(float measured)
{
    float error = target - measured;

    /*
      Error deadzone.
      Near a zero (or held) target, small encoder jitter/noise can
      produce a tiny nonzero error every cycle. Without this, that
      tiny PID output gets amplified by the motor driver's PWM
      deadband compensation (MOTOR_MIN_PWM) into a real, audible
      motor pulse - causing constant buzzing/chatter even when the
      robot should be sitting still. Ignoring error below this
      threshold keeps output at a clean 0 during genuine standstill,
      without affecting responsiveness once a real move command
      comes in (real error will be well above this threshold).
    */
    if (fabs(error) < 1.0)
    {
        integral = 0;
        previousError = error;
        return 0;
    }

    integral += error;

    /* Integral anti-windup */
    if (integral > 100.0) integral = 100.0;
    if (integral < -100.0) integral = -100.0;

    float derivative = error - previousError;
    float output = (kp * error) + (ki * integral) + (kd * derivative);
    previousError = error;

    if (output > outputLimit)  output = outputLimit;
    if (output < -outputLimit) output = -outputLimit;

    return output;
}

void PIDController::reset()
{
    integral = 0;
    previousError = 0;
}

void PIDController::setTunings(float p, float i, float d)
{
    kp = p;
    ki = i;
    kd = d;
}