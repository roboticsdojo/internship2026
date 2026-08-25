#include "watchdog.h"

unsigned long lastCommandTime = 0;

void watchdogReset()
{
    lastCommandTime = millis();
}

bool watchdogExpired()
{
    if (millis() - lastCommandTime > 2000)
    {
        return true;
    }
    return false;
}
