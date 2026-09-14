// encoder_driver.h
#ifndef ENCODER_DRIVER_H
#define ENCODER_DRIVER_H
#include <Arduino.h>

class EncoderDriver {
public:
    EncoderDriver(int pinA, int pinB);
    long getCount();
    void reset();
private:
    int _pinA, _pinB;
    volatile long _count = 0;
    static EncoderDriver* instanceL;
    static EncoderDriver* instanceR;
    static void isrL(); static void isrR();
};
#endif

