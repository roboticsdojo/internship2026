#include "imu_driver.h"
#include <Wire.h>
#include <MPU6050_light.h>

/*
  MPU6050_light (by rfetick) is used here rather than the heavier
  Adafruit_MPU6050 stack - it has no dependency chain (Adafruit's
  version pulls in Adafruit_Sensor + Adafruit_BusIO) and a much
  smaller memory footprint, which matters on the Uno's very limited
  2KB of SRAM alongside everything else this firmware already runs
  (String-based serial parsing, PID state, encoder counters).

  Install via Arduino IDE: Tools > Manage Libraries > search
  "MPU6050_light" by rfetick.
*/
MPU6050 mpu6050(Wire);

ImuDriver imu;

ImuDriver::ImuDriver()
{
    initialized_ = false;
}

bool ImuDriver::begin()
{
    Wire.begin();

    byte status = mpu6050.begin();

    if (status != 0)
    {
        initialized_ = false;
        return false;
    }

    /*
      Calibrates gyro/accel offsets. BLOCKS for ~2 seconds. The robot
      MUST be stationary and roughly level during this call - any
      motion or tilt here gets baked in as a permanent offset error.
    */
    mpu6050.calcOffsets();

    initialized_ = true;
    return true;
}

void ImuDriver::update()
{
    if (initialized_)
    {
        mpu6050.update();
    }
}

bool ImuDriver::isInitialized() const
{
    return initialized_;
}

/*
  MPU6050_light reports acceleration in g's - convert to m/s^2.
*/
float ImuDriver::getAccelX() const { return mpu6050.getAccX() * 9.80665f; }
float ImuDriver::getAccelY() const { return mpu6050.getAccY() * 9.80665f; }
float ImuDriver::getAccelZ() const { return mpu6050.getAccZ() * 9.80665f; }

/*
  MPU6050_light reports gyro rate in deg/s - convert to rad/s.
  DEG_TO_RAD is defined by the Arduino core (PI / 180.0).
*/
float ImuDriver::getGyroX() const { return mpu6050.getGyroX() * DEG_TO_RAD; }
float ImuDriver::getGyroY() const { return mpu6050.getGyroY() * DEG_TO_RAD; }
float ImuDriver::getGyroZ() const { return mpu6050.getGyroZ() * DEG_TO_RAD; }

/*
  MPU6050_light's getAngleX/Y/Z() is its complementary-filter
  roll/pitch/yaw estimate, in degrees - convert to radians.
*/
float ImuDriver::getRoll() const  { return mpu6050.getAngleX() * DEG_TO_RAD; }
float ImuDriver::getPitch() const { return mpu6050.getAngleY() * DEG_TO_RAD; }
float ImuDriver::getYaw() const   { return mpu6050.getAngleZ() * DEG_TO_RAD; }