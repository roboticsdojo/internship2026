// encoder_driver.cpp
#include "encoder_driver.h"
EncoderDriver* EncoderDriver::instanceL = nullptr;
EncoderDriver* EncoderDriver::instanceR = nullptr;

EncoderDriver::EncoderDriver(int pinA, int pinB) : _pinA(pinA), _pinB(pinB) {
    pinMode(_pinA, INPUT_PULLUP); pinMode(_pinB, INPUT_PULLUP);
}

void EncoderDriver::isrL() { if(instanceL) instanceL->_count += (digitalRead(instanceL->_pinB) == HIGH) ? 1 : -1; }
void EncoderDriver::isrR() { if(instanceR) instanceR->_count += (digitalRead(instanceR->_pinB) == HIGH) ? 1 : -1; }

long EncoderDriver::getCount() { return _count; }
void EncoderDriver::reset() { _count = 0; }