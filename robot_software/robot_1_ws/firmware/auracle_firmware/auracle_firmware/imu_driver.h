// imu_driver.h
#ifndef IMU_DRIVER_H
#define IMU_DRIVER_H
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

class IMUDriver {
public:
    bool begin();
    void readData(float* acc, float* gyro);
private:
    Adafruit_MPU6050 mpu;
};
#endif

