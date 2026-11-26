#include "serial_protocol.h"
#include <stdlib.h>
#include "config.h"

void SerialProtocol::begin() {
    Serial.begin(BAUDRATE);
}

void SerialProtocol::sendReady() {
    Serial.println(F("READY"));
}

bool SerialProtocol::poll(Command& cmd) {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\r') continue;
        if (c != '\n') {
            if (len_ < BUF_SIZE - 1) buf_[len_++] = c;
            else len_ = 0;  // overlong garbage - drop it
            continue;
        }
        if (len_ == 0) continue;
        buf_[len_] = '\0';
        len_ = 0;

        char* end;
        cmd.type = buf_[0];
        cmd.a = strtod(buf_ + 1, &end);
        cmd.b = strtod(end, &end);
        return true;
    }
    return false;
}

void SerialProtocol::sendTelemetry(unsigned long ms, long l, long r, const int16_t* imu) {
    // Integers only - float printing is slow on AVR.
    Serial.print('e');
    Serial.print(' '); Serial.print(ms);
    Serial.print(' '); Serial.print(l);
    Serial.print(' '); Serial.print(r);
    for (uint8_t i = 0; i < 6; i++) {
        Serial.print(' ');
        Serial.print(imu[i]);
    }
    Serial.print('\n');
}

void SerialProtocol::sendFault(char side, uint8_t code, int speed, int target) {
    Serial.print('F');
    Serial.print(' '); Serial.print(side);
    Serial.print(' '); Serial.print(code);
    Serial.print(' '); Serial.print(speed);
    Serial.print(' '); Serial.print(target);
    Serial.print('\n');
}