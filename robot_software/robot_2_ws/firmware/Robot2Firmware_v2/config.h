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
 IMU (MPU6050, I2C)
================================================
 SDA = A4 (fixed on Uno - I2C hardware pin, not configurable)
 SCL = A5 (fixed on Uno - I2C hardware pin, not configurable)
 VCC = 5V (from buck converter rail - see robot_2_hardware README
       for why this is NOT powered from the Arduino's own 5V pin
       or the L298's onboard regulator)
 GND = common ground (Arduino GND + L298 GNDs + encoder GND + IMU GND)
*/

/*
================================================
 RESERVED
================================================
*/

// Future servo
#define SERVO_PIN 11

/*
================================================
 ROBOT PARAMETERS
================================================
*/

#define ENCODER_TICKS_PER_REV 1980
#define MOTOR_MAX_PWM 255

// Measured minimum PWM from testing. Set to 0 to disable deadband
// compensation entirely (see motor_driver.cpp) - this was done on the
// current chassis after PID retuning revealed the compensation was
// causing oscillation at low commanded speeds.
#define MOTOR_MIN_PWM 0

/*
 PID
*/
#define PID_PERIOD 50   // ms

/*
 SERIAL
*/
#define SERIAL_BAUD 57600

#endif