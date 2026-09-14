#ifndef IMU_DRIVER_H
#define IMU_DRIVER_H

#include <Arduino.h>

/*
  Thin wrapper around the MPU6050_light library, converting its native
  units (g's, deg/s, degrees) to the SI units ROS2/ros2_control expect
  (m/s^2, rad/s, radians). Keeping the unit conversion here means
  nothing upstream (serial_protocol.cpp, and eventually the
  robot_2_hardware plugin) needs to know or care what units the
  library itself uses internally.
*/
class ImuDriver
{
public:
    ImuDriver();

    /*
      Initializes I2C and the MPU6050, then runs the library's
      calcOffsets() gyro/accel calibration routine. This BLOCKS for a
      couple of seconds and REQUIRES the robot to be stationary and
      level - call this once in setup(), before the robot could
      possibly be moving.
      Returns false if the MPU6050 did not respond on the I2C bus
      (check wiring: SDA=A4, SCL=A5, VCC, GND).
    */
    bool begin();

    /*
      Refreshes the underlying complementary filter. Must be called
      frequently (every loop() iteration) for the roll/pitch/yaw
      estimate to stay accurate - NOT just when an 'i' command comes
      in over serial, since the filter needs a consistent update rate
      independent of how often the Pi happens to poll it.
    */
    void update();

    bool isInitialized() const;

    // Linear acceleration, m/s^2
    float getAccelX() const;
    float getAccelY() const;
    float getAccelZ() const;

    // Angular velocity, rad/s
    float getGyroX() const;
    float getGyroY() const;
    float getGyroZ() const;

    // Orientation (complementary filter estimate), radians.
    // NOTE: yaw will drift over time - the MPU6050 has no
    // magnetometer, so there is nothing to correct yaw drift against.
    // Roll/pitch are stable (gravity-referenced).
    float getRoll() const;
    float getPitch() const;
    float getYaw() const;

private:
    bool initialized_;
};

extern ImuDriver imu;

#endif