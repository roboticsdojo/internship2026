#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <Arduino.h>
#include <avr/wdt.h> // Standard AVR library for Nano/Uno

class Watchdog {
public:
    // Starts the hardware timer (timeout in milliseconds, mapped to the
    // nearest AVR WDTO_ setting - valid range is roughly 15ms..8000ms).
    // NOTE: the old signature took this as uint8_t, which silently
    // truncated any value above 255 (e.g. the intended 2000ms default
    // became 2000 % 256 = 208ms) - fixed to uint16_t.
    void begin(uint16_t timeout_ms = 2000);

    // "Pets" the dog to prevent reset
    void pet();

    // Communication Watchdog: checks if we've heard from ROS recently
    bool isCommsAlive(unsigned long lastPacketTime, unsigned long timeout = 500);
};

#endif
