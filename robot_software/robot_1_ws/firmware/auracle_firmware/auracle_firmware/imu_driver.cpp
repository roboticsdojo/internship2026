// imu_driver.cpp
#include "imu_driver.h"
bool IMUDriver::begin() { return mpu.begin(); }
void IMUDriver::readData(float* acc, float* gyro) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    acc[0] = a.acceleration.x; acc[1] = a.acceleration.y; acc[2] = a.acceleration.z;
    gyro[0] = g.gyro.x; gyro[1] = g.gyro.y; gyro[2] = g.gyro.z;
}