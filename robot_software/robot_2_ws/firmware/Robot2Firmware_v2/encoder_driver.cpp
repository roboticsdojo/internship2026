#include "encoder_driver.h"

EncoderDriver encoders;

// Define static members here
volatile long EncoderDriver::leftCounter = 0;
volatile long EncoderDriver::rightCounter = 0;

/*
 LEFT encoder interrupt
 Triggers on CHANGE of channel A (Pin 2).
 Reads channel B (Pin 4) to resolve direction.
*/
void leftEncoderISR()
{
    int aState = digitalRead(LEFT_ENCODER_A);
    int bState = digitalRead(LEFT_ENCODER_B);

    if (aState == bState)
    {
        EncoderDriver::leftCounter--;
    }
    else
    {
        EncoderDriver::leftCounter++;
    }
}

/*
 RIGHT encoder interrupt
 Triggers on CHANGE of channel A (Pin 3).
 Reads channel B (Pin 12) to resolve direction.
*/
void rightEncoderISR()
{
    int aState = digitalRead(RIGHT_ENCODER_A);
    int bState = digitalRead(RIGHT_ENCODER_B);

    if (aState == bState)
    {
        EncoderDriver::rightCounter++;
    }
    else
    {
        EncoderDriver::rightCounter--;
    }
}

EncoderDriver::EncoderDriver()
{
    lastLeftTicks = 0;
    lastRightTicks = 0;
    leftDelta = 0;
    rightDelta = 0;
}

void EncoderDriver::begin()
{
    pinMode(LEFT_ENCODER_A, INPUT_PULLUP);
    pinMode(LEFT_ENCODER_B, INPUT_PULLUP);
    pinMode(RIGHT_ENCODER_A, INPUT_PULLUP);
    pinMode(RIGHT_ENCODER_B, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(LEFT_ENCODER_A), leftEncoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(RIGHT_ENCODER_A), rightEncoderISR, CHANGE);
}

long EncoderDriver::getLeftTicks()
{
    noInterrupts();
    long value = leftCounter;
    interrupts();
    return value;
}

long EncoderDriver::getRightTicks()
{
    noInterrupts();
    long value = rightCounter;
    interrupts();
    return value;
}

void EncoderDriver::reset()
{
    noInterrupts();
    leftCounter = 0;
    rightCounter = 0;
    interrupts();

    lastLeftTicks = 0;
    lastRightTicks = 0;
    leftDelta = 0;
    rightDelta = 0;
}

void EncoderDriver::update()
{
    long currentLeft = getLeftTicks();
    long currentRight = getRightTicks();

    leftDelta = currentLeft - lastLeftTicks;
    rightDelta = currentRight - lastRightTicks;

    lastLeftTicks = currentLeft;
    lastRightTicks = currentRight;
}

long EncoderDriver::getLeftDelta()
{
    return leftDelta;
}

long EncoderDriver::getRightDelta()
{
    return rightDelta;
}
