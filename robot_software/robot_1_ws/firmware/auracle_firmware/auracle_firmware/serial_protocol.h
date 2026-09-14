// serial_protocol.h
#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H
#include <Arduino.h>

class SerialProtocol {
public:
    // Sends "READY\n" once, called from setup() after everything else is
    // initialized, so the hardware-interface handshake (ArduinoComms::waitForReady)
    // has something to wait for.
    void sendReady();

    // Non-blocking: accumulates characters into an internal buffer across
    // repeated calls and only returns true once a full "v <l> <r>\n" line
    // has arrived. Never blocks, unlike Serial.readBytesUntil().
    bool parseVelocity(double &left, double &right);

    void sendTelemetry(long l_enc, long r_enc, float* acc, float* gyro);

private:
    static const uint8_t BUF_SIZE = 32;
    char buf_[BUF_SIZE];
    uint8_t buf_len_ = 0;
};
#endif
