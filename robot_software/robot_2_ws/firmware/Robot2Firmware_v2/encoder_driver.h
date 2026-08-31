#ifndef ENCODER_DRIVER_H
#define ENCODER_DRIVER_H

#include <Arduino.h>
#include "config.h"

class EncoderDriver
{
public:
    EncoderDriver();
    void begin();
    long getLeftTicks();
    long getRightTicks();
    void reset();
    void update();
    long getLeftDelta();
    long getRightDelta();

    // Static variables accessed directly by the ISRs
    static volatile long leftCounter;
    static volatile long rightCounter;

private:
    long lastLeftTicks;
    long lastRightTicks;
    long leftDelta;
    long rightDelta;
};

extern EncoderDriver encoders;

/*
 Interrupt service routine signatures
*/
void leftEncoderISR();
void rightEncoderISR();

#endif
