#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/*
================================================
 MOTOR DRIVER PINS (L298 Drivers)
================================================
*/

// LEFT SIDE L298 (Controls Both Left Motors)
#define LEFT_PWM      5  // Connects to ENA or ENB (Speed)
#define LEFT_IN1      7  // Connects to IN1 or IN3 (Direction)
#define LEFT_IN2      8  // Connects to IN2 or IN4 (Direction)

// RIGHT SIDE L298 (Controls Both Right Motors)
#define RIGHT_PWM     6  // Connects to ENA or ENB (Speed)
#define RIGHT_IN1     9  // Connects to IN1 or IN3 (Direction)
#define RIGHT_IN2     10 // Connects to IN2 or IN4 (Direction)

/*
================================================
 ENCODERS
================================================
*/

#define LEFT_ENCODER_A   2  // Must be pin 2 (Hardware Interrupt INT0)
#define LEFT_ENCODER_B   4  // Digital pin

#define RIGHT_ENCODER_A  3  // Must be pin 3 (Hardware Interrupt INT1)
#define RIGHT_ENCODER_B  12 // Digital pin

/*
================================================
 RESERVED
================================================
*/

// Future servo
#define SERVO_PIN 11

// MPU6050
// SDA = A4
// SCL = A5

/*
================================================
 ROBOT PARAMETERS
================================================
*/

#define ENCODER_TICKS_PER_REV 1980
#define MOTOR_MAX_PWM 255

// Measured minimum PWM from testing
#define MOTOR_MIN_PWM 60

/*
 PID
*/
#define PID_PERIOD 50   // ms

/*
 SERIAL
*/
#define SERIAL_BAUD 57600

#endif
