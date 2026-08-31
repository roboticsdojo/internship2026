#include "motor_driver.h"

/*
================================================
 Initialize L298 Drivers
================================================
*/
void motorBegin()
{
    pinMode(LEFT_PWM, OUTPUT);
    pinMode(LEFT_IN1, OUTPUT);
    pinMode(LEFT_IN2, OUTPUT);

    pinMode(RIGHT_PWM, OUTPUT);
    pinMode(RIGHT_IN1, OUTPUT);
    pinMode(RIGHT_IN2, OUTPUT);

    motorSetPWM(0, 0);
}

/*
================================================
 LEFT SIDE CONTROL
================================================
*/
void setLeftMotor(int pwm)
{
    bool reverse = false;

    if (pwm < 0)
    {
        reverse = true;
        pwm = -pwm;
    }

    if (pwm > MOTOR_MAX_PWM)
        pwm = MOTOR_MAX_PWM;

    /*
      Deadband compensation.
      Below MOTOR_MIN_PWM the motor draws current but does not
      actually turn, which stalls the PID loop (it sees zero
      ticks and keeps ramping error/integral). Any nonzero
      command is bumped up to the measured minimum that actually
      moves the wheels.
    */
    if (pwm > 0 && pwm < MOTOR_MIN_PWM)
        pwm = MOTOR_MIN_PWM;

    if (reverse)
    {
        digitalWrite(LEFT_IN1, LOW);
        digitalWrite(LEFT_IN2, HIGH);
    }
    else
    {
        digitalWrite(LEFT_IN1, HIGH);
        digitalWrite(LEFT_IN2, LOW);
    }

    analogWrite(LEFT_PWM, pwm);
}

/*
================================================
 RIGHT SIDE CONTROL
================================================
*/
void setRightMotor(int pwm)
{
    bool reverse = false;

    if (pwm < 0)
    {
        reverse = true;
        pwm = -pwm;
    }

    if (pwm > MOTOR_MAX_PWM)
        pwm = MOTOR_MAX_PWM;

    /*
      Deadband compensation - see setLeftMotor() above.
    */
    if (pwm > 0 && pwm < MOTOR_MIN_PWM)
        pwm = MOTOR_MIN_PWM;

    if (reverse)
    {
        digitalWrite(RIGHT_IN1, LOW);
        digitalWrite(RIGHT_IN2, HIGH);
    }
    else
    {
        digitalWrite(RIGHT_IN1, HIGH);
        digitalWrite(RIGHT_IN2, LOW);
    }

    analogWrite(RIGHT_PWM, pwm);
}

/*
================================================
 Main interface used by PID
================================================
*/
void motorSetPWM(int leftPWM, int rightPWM)
{
    setLeftMotor(leftPWM);
    setRightMotor(rightPWM);
}
