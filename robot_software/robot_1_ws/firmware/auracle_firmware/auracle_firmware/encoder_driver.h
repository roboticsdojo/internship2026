#pragma once
#include <Arduino.h>

// 2x quadrature decoding. Attach tick() to a CHANGE interrupt on the INT pin (channel A);
// the DIR pin is channel B. On every edge of A, A == B means one direction and A != B the other.
// `sign` is added when A == B and subtracted otherwise (config.h: ENC_SIGN, measured -1 for both rears).
class EncoderDriver {
public:
    EncoderDriver(uint8_t pinInt, uint8_t pinDir, int8_t sign)
        : pinInt_(pinInt), pinDir_(pinDir), sign_(sign) {}

    void begin() {
        pinMode(pinInt_, INPUT_PULLUP);
        pinMode(pinDir_, INPUT_PULLUP);
        intPort_ = portInputRegister(digitalPinToPort(pinInt_));
        intMask_ = digitalPinToBitMask(pinInt_);
        dirPort_ = portInputRegister(digitalPinToPort(pinDir_));
        dirMask_ = digitalPinToBitMask(pinDir_);
    }

    // ISR: two port reads instead of digitalRead() (~4 us each).
    void tick() {
        bool a = (*intPort_ & intMask_) != 0;
        bool b = (*dirPort_ & dirMask_) != 0;
        if (a == b) count_ += sign_;
        else        count_ -= sign_;
    }

    long count() {
        noInterrupts();  // 32-bit read isn't atomic on AVR
        long c = count_;
        interrupts();
        return c;
    }

    uint8_t interruptPin() const { return pinInt_; }

    // B-only mode (channel A unreliable): call tickB() on every edge of the DIR pin (channel B).
    // Each B edge is one count, the same scale as 2x decoding on A. Direction can't come from A,
    // so it comes from setDirection(): the side's current drive direction (0 = keep the last one).
    void tickB() { count_ += dirHint_; }
    void setDirection(int8_t d) { if (d != 0) dirHint_ = d; }

private:
    uint8_t pinInt_, pinDir_;
    int8_t sign_;
    volatile uint8_t* intPort_ = nullptr;
    volatile uint8_t* dirPort_ = nullptr;
    uint8_t intMask_ = 0, dirMask_ = 0;
    volatile long count_ = 0;
    volatile int8_t dirHint_ = 1;
};