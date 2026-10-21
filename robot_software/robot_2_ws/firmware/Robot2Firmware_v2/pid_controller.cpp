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
      Error deadzone - STANDSTILL ONLY.
      Near a zero target, small encoder jitter/noise can produce a tiny
      nonzero error every cycle. Without this, that tiny PID output gets
      amplified by the motor driver's PWM deadband compensation
      (MOTOR_MIN_PWM) into a real, audible motor pulse - causing constant
      buzzing/chatter even when the robot should be sitting still.
      Ignoring error below this threshold keeps output at a clean 0 during
      genuine standstill.

      CHANGED: this used to apply for ANY target. At a slow nonzero target
      (e.g. a slow in-place turn, about 8 ticks per PID period) the measured
      speed is often within 1 tick of the target, and each time that happened
      the output was forced to 0 and the integral wiped, so the drive kept
      losing the push it needed to keep the wheels moving. It now only applies
      when the target itself is (almost) zero.
    */
    if (fabs(target) < 1.0 && fabs(error) < 1.0)
    {
        integral = 0;
        previousError = error;
        return 0;
    }

    integral += error;

    /*
      Integral anti-windup.

      CHANGED: the limit used to be a fixed +/-100, which capped the
      integral term's contribution at ki * 100 (= 120 PWM with ki = 1.2).
      At a small target (e.g. a slow in-place turn, about 8 ticks per
      PID period) the proportional term only adds kp * 8 = 16, so the
      output could never exceed roughly 136 PWM. A 4-wheel skid-steer
      scrubbing on the floor can need more than that to start turning,
      so slow commands stalled while faster ones (bigger P term) worked.

      The limit is now derived from the output limit: the integral term
      alone may reach full PWM (outputLimit / ki), and it is still
      clamped, so it cannot wind up without bound.
    */
    float integralLimit = (ki > 0.0f) ? (outputLimit / ki) : 0.0f;
    if (integral > integralLimit)  integral = integralLimit;
    if (integral < -integralLimit) integral = -integralLimit;

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