#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <Arduino.h>

extern unsigned long lastCommandTime;

void watchdogReset();
bool watchdogExpired();

#endif
