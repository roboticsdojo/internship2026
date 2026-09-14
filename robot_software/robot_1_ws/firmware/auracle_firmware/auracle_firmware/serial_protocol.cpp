// serial_protocol.cpp
#include "serial_protocol.h"

void SerialProtocol::sendReady() {
    Serial.println("READY");
}

bool SerialProtocol::parseVelocity(double &left, double &right) {
    // Drain whatever bytes are currently available without ever blocking -
    // this is called once per main loop() iteration, so the PID/watchdog
    // timing stays consistent regardless of how fast bytes arrive.
    while (Serial.available() > 0) {
        char c = (char)Serial.read();

        if (c == '\n') {
            buf_[buf_len_] = '\0';
            bool parsed = false;
            if (buf_len_ > 0 && buf_[0] == 'v') {
                if (sscanf(buf_, "v %lf %lf", &left, &right) == 2) {
                    parsed = true;
                }
            }
            buf_len_ = 0;
            if (parsed) {
                return true;
            }
            continue;
        }

        if (buf_len_ < BUF_SIZE - 1) {
            buf_[buf_len_++] = c;
        } else {
            // Line too long / garbage - drop it and resync on the next '\n'.
            buf_len_ = 0;
        }
    }
    return false;
}

void SerialProtocol::sendTelemetry(long l_enc, long r_enc, float* acc, float* gyro) {
    Serial.print("e "); Serial.print(l_enc); Serial.print(" "); Serial.print(r_enc);
    Serial.print(" i ");
    for (int i = 0; i < 3; i++) { Serial.print(acc[i]); Serial.print(" "); }
    for (int i = 0; i < 3; i++) { Serial.print(gyro[i]); Serial.print(i == 2 ? "" : " "); }
    Serial.println();
}
