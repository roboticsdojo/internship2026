#ifndef CONFIG_H
#define CONFIG_H

// --- Motor Pins (L298N) ---
// NOTE: L_MOTOR_IN2 was previously pin 3, which is the SAME physical pin as
// R_ENC_A below (the right encoder's hardware interrupt). Whichever
// constructor ran later at static-init time (EncoderDriver, after both
// MotorDrivers) silently switched pin 3 back to INPUT_PULLUP, so the right
// motor's direction line stopped working correctly and fought the encoder
// ISR on the same pin. Moved to pin 9, the only digital pin that wasn't
// already claimed by something else.
#define L_MOTOR_PWM 5
#define L_MOTOR_IN1 4
#define L_MOTOR_IN2 9
#define R_MOTOR_PWM 6
#define R_MOTOR_IN1 7
#define R_MOTOR_IN2 8

// --- Encoder Pins ---
// L_ENC_A/R_ENC_A MUST stay on 2 and 3 - those are the only two pins on the
// Nano's ATmega328P with hardware external-interrupt support.
#define L_ENC_A 2  // Interrupt pin
#define L_ENC_B 10
#define R_ENC_A 3  // Interrupt pin
#define R_ENC_B 11

// --- Constants ---
// Must match ros2_control.xacro's <param name="baud_rate"> and
// auracle_hardware's "baud_rate" hardware_parameter.
#define BAUDRATE 115200
// Must match ros2_control.xacro's <param name="enc_counts_per_rev"> - this
// is how auracle_hardware converts ticks <-> radians, so a mismatch here
// silently scales all wheel odometry/velocity.
#define PPR 3436.0        // Pulses per revolution
#define WHEEL_RADIUS 0.0239
#define PID_KP 1.5
#define PID_KI 0.1
#define PID_KD 0.01

// ROS sends target velocity in rad/s (matches the diff_drive_controller's
// command interface). This converts rad/s -> encoder ticks/s so the PID
// setpoint and the measured speed (also in ticks/s) are in the same units.
#define RAD_S_TO_TICKS_S(rad_s) ((rad_s) * PPR / (2.0 * PI))

#endif
