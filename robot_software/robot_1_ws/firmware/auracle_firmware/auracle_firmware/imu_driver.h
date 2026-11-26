#pragma once
#include <Arduino.h>

// Raw-register MPU6050 driver. This clone reports WHO_AM_I = 0x72, which
// the Adafruit library rejects, so the registers are driven directly.
class IMUDriver {
public:
    void begin();
    // Averages the gyro at rest; robot must be still.
    void calibrateGyro(uint16_t samples);
    // out = ax ay az gx gy gz, raw counts (gyro bias removed).
    // On a failed read, out keeps its previous values.
    void read(int16_t* out);
    // WHO_AM_I register (0x68 genuine MPU6050, 0x70/0x72 clones), 0 = no answer.
    uint8_t whoAmI();

private:
    int16_t gyroBias_[3] = {0, 0, 0};
};
