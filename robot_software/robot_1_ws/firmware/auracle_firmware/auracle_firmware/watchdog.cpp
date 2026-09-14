#include "watchdog.h"

namespace {
uint8_t msToWdto(uint16_t timeout_ms) {
    if (timeout_ms <= 15) return WDTO_15MS;
    if (timeout_ms <= 30) return WDTO_30MS;
    if (timeout_ms <= 60) return WDTO_60MS;
    if (timeout_ms <= 120) return WDTO_120MS;
    if (timeout_ms <= 250) return WDTO_250MS;
    if (timeout_ms <= 500) return WDTO_500MS;
    if (timeout_ms <= 1000) return WDTO_1S;
    if (timeout_ms <= 2000) return WDTO_2S;
    if (timeout_ms <= 4000) return WDTO_4S;
    return WDTO_8S;
}
}  // namespace

void Watchdog::begin(uint16_t timeout_ms) {
    wdt_enable(msToWdto(timeout_ms));
}

void Watchdog::pet() {
    wdt_reset();
}

bool Watchdog::isCommsAlive(unsigned long lastPacketTime, unsigned long timeout) {
    if (millis() - lastPacketTime > timeout) {
        return false; // Comms are dead!
    }
    return true;
}
