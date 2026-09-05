#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <Arduino.h>
#include "config.h"

void motorBegin();

void motorSetPWM(
    int leftPWM,
    int rightPWM
);

void setLeftMotor(
    int pwm
);

void setRightMotor(
    int pwm
);

#endif

/*
******** LEFT DRIVER ********
 One L298 controls:
 Left Front Motor + Left Rear Motor
 ---- pinout -----
 ENA + ENB = 5 PWM (Speed)
 IN1 + IN3 = 7 (Direction)
 IN2 + IN4 = 8 (Direction)

******** RIGHT DRIVER ********
 One L298 controls:
 Right Front Motor + Right Rear Motor
 ---- pinout -----
 ENA + ENB = 6 PWM (Speed)
 IN1 + IN3 = 9 (Direction)
 IN2 + IN4 = 10 (Direction)

 ******* Encoders ********
 ---- REAR LEFT ENCODER ----
 Channel A - 2
 Channel B - 4

 ---- REAR RIGHT ENCODER ----
 Channel A - 3
 Channel B - 12

 NOTE: The encoder vcc - 5V and GND - GND

 ******* IMU (MPU6050, I2C) *******
 SDA - A4 (fixed I2C pin on the Uno, not configurable)
 SCL - A5 (fixed I2C pin on the Uno, not configurable)
 VCC - 5V (from the buck converter rail - NOT the Arduino's own
       5V pin and NOT the L298's onboard regulator, same reasoning
       as the encoders above)
 GND - GND

 ******* GND SHARING *******
Arduino GND + both L298 Motor driver GND + Encoder GND + IMU GND
*/