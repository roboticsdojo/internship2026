#pragma once
#include <Arduino.h>

// ASCII lines, '\n' terminated.
// Pi -> Nano:
//   v <left_ticks_s> <right_ticks_s>   closed-loop velocity target for the rear wheels
//                                      (1x ticks/s; |v| below MIN_TARGET_TICKS_S = stop)
//   o <left_pwm> <right_pwm>           open-loop rear PWM, fronts follow (front-offset test)
//   p <LB> <LF> / q <RB> <RF>          TEST: raw signed PWM per wheel (no trim/floor/soft start)
//   g <kp> <ki> / h <kp> <ki>          TEST: live left / right PI gains (not saved)
//   s                                  stop, and clear a latched fault
//                                      (a 'v 0 0' also clears a latched fault)
// Nano -> Pi:
//   READY                              once after boot
//   e <ms> <l_ticks> <r_ticks> <ax> <ay> <az> <gx> <gy> <gz>   every 50 ms
//   F <L|R> <code> <speed> <target>    wheel fault (speed = filtered 1x ticks/s, target = commanded), motors stopped until 's' or 'v 0 0'
//                                      (1 = moving against the command, 2 = PWM saturated, target not reached,
//                                       3 = no motion: dead encoder or blocked wheel)
//                                      only sent when REPORT_FAULTS is true in config.h
struct Command {
    char type = 0;
    float a = 0.0f, b = 0.0f;
};

class SerialProtocol {
public:
    void begin();
    void sendReady();
    // True once per complete line; call in a loop to drain.
    bool poll(Command& cmd);
    void sendTelemetry(unsigned long ms, long l, long r, const int16_t* imu);
    void sendFault(char side, uint8_t code, int speed, int target);

private:
    static constexpr uint8_t BUF_SIZE = 32;
    char buf_[BUF_SIZE];
    uint8_t len_ = 0;
};