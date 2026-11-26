#pragma once
#include <Arduino.h>
#include "config.h"

// Per-wheel constants (built from config.h in the main sketch).
struct WheelParams {
    uint8_t kickF, kickR;       // PWM held from rest (forward / reverse)
    uint8_t floorF, floorR;     // lowest PWM while turning
    float kp, ki;
    float ffBaseF, ffKF;        // feedforward PWM = base + k * |ticks/s|, forward
    float ffBaseR, ffKR;        // ... reverse
};

enum DriveMode : uint8_t { DRIVE_COAST, DRIVE_RUN, DRIVE_BRAKE };
struct DriveCmd {
    DriveMode mode;
    int effort;                 // signed rear PWM, only meaningful for DRIVE_RUN
};

enum Fault : uint8_t { FAULT_NONE = 0, FAULT_WRONG_SIGN = 1, FAULT_SATURATED = 2, FAULT_NO_MOTION = 3 };

// Closed-loop speed control for one rear wheel. Each control period:
//   ff  = BASE + K * rampedTarget            (follows the ramped setpoint)
//   out = ff + Kp*err + integral             (integral clamped, conditional anti-windup)
// Starting from rest holds a kick PWM until the wheel turns. Stopping or reversing brakes
// briefly, then coasts to standstill before driving the other way (the H-bridge direction is
// never flipped at speed). Never blocks.
class PIDController {
public:
    explicit PIDController(const WheelParams& p) : p_(p) {}

    // Keep the speed filter running while the controller is not in charge (open-loop mode).
    void observe(float rawSpeed) { speed_ += SPEED_FILTER_ALPHA * (rawSpeed - speed_); }

    // target and rawSpeed are signed 1x ticks/s (positive = forward). Returns what to drive.
    DriveCmd update(float target, float rawSpeed, float dt, unsigned long ms) {
        observe(rawSpeed);
        if (fault_ != FAULT_NONE) return coast();

        int8_t want = (target > 0.0f) ? 1 : ((target < 0.0f) ? -1 : 0);

        // Stop or reversal while running: brake if still rolling, else go straight to the wait.
        if (state_ == ST_RUN && want != dir_) {
            if (BRAKE_MS > 0 && fabs(speed_) > BRAKE_MIN_TICKS_S) {
                state_ = ST_BRAKE;
                phaseEnd_ = ms + BRAKE_MS;
            } else {
                state_ = ST_WAIT;
                phaseEnd_ = ms + REVERSE_WAIT_MS;
            }
        }
        if (state_ == ST_BRAKE) {
            if (before(ms, phaseEnd_)) return brake();
            state_ = ST_WAIT;
            phaseEnd_ = ms + REVERSE_WAIT_MS;
        }
        if (state_ == ST_WAIT) {
            // Only a reversal has to wait for standstill; stopping or resuming the same way doesn't.
            bool reversing = (want != 0) && (want == -dir_);
            if (reversing && fabs(speed_) > STILL_TICKS_S && before(ms, phaseEnd_)) return coast();
            state_ = ST_IDLE;
        }
        if (state_ == ST_IDLE) {
            if (want == 0) return coast();
            beginRun(want, ms);
        }

        // ---- ST_RUN ----
        float absT = fabs(target);

        // setpoint ramp toward |target|
        float stepT = TARGET_RAMP_TICKS_S2 * dt;
        if (rampedAbs_ < absT) {
            rampedAbs_ += stepT;
            if (rampedAbs_ > absT) rampedAbs_ = absT;
        } else {
            rampedAbs_ -= stepT;
            if (rampedAbs_ < absT) rampedAbs_ = absT;
        }

        float spd      = dir_ * speed_;      // positive = moving as commanded
        float floorPwm = (dir_ > 0) ? p_.floorF : p_.floorR;
        float kickPwm  = (dir_ > 0) ? p_.kickF : p_.kickR;
        float ffPwm    = min(feedforward(rampedAbs_) + boost_, 255.0f);

        // From rest: hold at least the kick PWM until the wheel is turning (control paused), then
        // hand over at no less than the feedforward, so the output never drops back to the floor.
        if (!kickDone_) {
            if (before(ms, kickEnd_) && fabs(speed_) < KICK_EXIT_TICKS_S) {
                float c = (out_ > kickPwm) ? out_ : kickPwm;
                return run((int)(c + 0.5f));
            }
            kickDone_ = true;
            float hand = ffPwm;
            if (fabs(speed_) < KICK_EXIT_TICKS_S && kickPwm > hand) hand = kickPwm;   // still stalled
            if (hand > out_) out_ = hand;
        }

        float err = constrain(rampedAbs_ - spd, -ERR_CLAMP_TICKS_S, ERR_CLAMP_TICKS_S);

        // integral with conditional anti-windup (no further integration into saturation)
        float u = ffPwm + p_.kp * err + integ_;
        bool satHi = (u > 255.0f) && (err > 0.0f);
        bool satLo = (u < floorPwm) && (err < 0.0f);
        if (!satHi && !satLo) integ_ = constrain(integ_ + p_.ki * err * dt, -INTEG_MAX_PWM, INTEG_MAX_PWM);
        u = ffPwm + p_.kp * err + integ_;

        float delta = constrain(u - out_, -MAX_PWM_STEP, MAX_PWM_STEP);
        out_ = constrain(out_ + delta, floorPwm, 255.0f);

        // Fault detection (an unplugged or reversed encoder would otherwise run the wheel away).
        if (!before(ms, graceEnd_)) {
            if (spd < -FAULT_WRONG_SIGN_TICKS_S) {
                if (++wrongSign_ >= FAULT_WRONG_SIGN_COUNT) { fault_ = FAULT_WRONG_SIGN; return coast(); }
            } else wrongSign_ = 0;

            if (out_ >= 254.5f && spd < 0.8f * absT) {
                if (++satCount_ >= FAULT_SATURATED_COUNT) {
                    satCount_ = 0;
                    if (!FAULT_STOP_ON_STALL) report_ = FAULT_SATURATED;   // log it, keep driving
                    else { fault_ = FAULT_SATURATED; return coast(); }
                }
            } else satCount_ = 0;

            // Dead encoder or blocked wheel: PWM is applied but the wheel barely moves. The integral
            // clamp keeps the PWM below 255 here, so the saturation check above never sees it.
            if (rampedAbs_ > 0.0f && spd < FAULT_NO_MOTION_FRAC * rampedAbs_) {
                if (++noMotion_ >= FAULT_NO_MOTION_COUNT) {
                    noMotion_ = 0;
                    if (!FAULT_STOP_ON_STALL) report_ = FAULT_NO_MOTION;   // log it, keep driving
                    else { fault_ = FAULT_NO_MOTION; return coast(); }
                }
            } else noMotion_ = 0;
        }

        // Above the target by a wide margin: coast (a PWM floor can't slow the wheel down).
        if (spd > absT * OVERSPEED_COAST_RATIO) return coast();
        return run((int)(out_ + 0.5f));
    }

    // Back to idle (state, integral, ramp). The speed filter and any fault are kept.
    void reset() {
        state_ = ST_IDLE;
        dir_ = 0;
        out_ = integ_ = rampedAbs_ = 0.0f;
        wrongSign_ = satCount_ = noMotion_ = 0;
    }

    // Live gain change for tuning ('g'/'h' serial commands). Lost on reset; copy results to config.h.
    void setGains(float kp, float ki) { p_.kp = kp; p_.ki = ki; }

    // Extra feedforward PWM (turn boost), set every control period. 0 = none.
    void setBoost(float b) { boost_ = b; }

    float speed() const { return speed_; }
    Fault fault() const { return fault_; }
    void clearFault() { fault_ = FAULT_NONE; }
    // A stall seen while FAULT_STOP_ON_STALL is false: reported once, nothing is stopped.
    Fault takeReport() { Fault r = report_; report_ = FAULT_NONE; return r; }

private:
    enum State : uint8_t { ST_IDLE, ST_RUN, ST_BRAKE, ST_WAIT };

    static bool before(unsigned long a, unsigned long b) { return (long)(a - b) < 0; }

    static DriveCmd coast() { return DriveCmd{DRIVE_COAST, 0}; }
    static DriveCmd brake() { return DriveCmd{DRIVE_BRAKE, 0}; }
    DriveCmd run(int pwm) const {
        if (pwm <= 0) return coast();
        if (pwm > 255) pwm = 255;
        return DriveCmd{DRIVE_RUN, dir_ * pwm};
    }

    float feedforward(float ramped) const {
        float base = (dir_ > 0) ? p_.ffBaseF : p_.ffBaseR;
        float k    = (dir_ > 0) ? p_.ffKF : p_.ffKR;
        float flr  = (dir_ > 0) ? p_.floorF : p_.floorR;
        return constrain(base + k * ramped, flr, 255.0f);
    }

    void beginRun(int8_t want, unsigned long ms) {
        dir_ = want;
        state_ = ST_RUN;
        // already rolling the right way (e.g. resuming after a stop)? start the ramp from there
        float moving = dir_ * speed_;
        rampedAbs_ = (moving > 0.0f) ? moving : 0.0f;
        integ_ = 0.0f;
        out_ = feedforward(rampedAbs_);
        kickDone_ = false;
        kickEnd_ = ms + KICK_MS;
        wrongSign_ = satCount_ = noMotion_ = 0;
        graceEnd_ = ms + FAULT_GRACE_MS;
    }

    WheelParams p_;
    float boost_ = 0.0f;
    State state_ = ST_IDLE;
    int8_t dir_ = 0;                 // direction of the last / current run
    float speed_ = 0.0f;             // filtered, signed ticks/s
    float integ_ = 0.0f, out_ = 0.0f, rampedAbs_ = 0.0f;
    bool kickDone_ = true;
    unsigned long kickEnd_ = 0, phaseEnd_ = 0, graceEnd_ = 0;
    uint8_t wrongSign_ = 0, satCount_ = 0, noMotion_ = 0;
    Fault fault_ = FAULT_NONE;
    Fault report_ = FAULT_NONE;
};
// Drives a wheel with no usable encoder: feedforward PWM on a ramped setpoint, the same kick
// from rest as the PI, and no fault checks (they all need a speed reading). It also keeps an
// estimate of the wheel's position from the ramped setpoint, sent in place of encoder counts
// so the Pi's odometry still has something sensible for that side.
class OpenLoopWheel {
public:
    OpenLoopWheel(const WheelParams& p, float gainF, float gainR) : p_(p), gainF_(gainF), gainR_(gainR) {}

    // target: signed 1x ticks/s. Passing through zero ramps down, coasts briefly, ramps up.
    DriveCmd update(float target, float dt, unsigned long ms) {
        float step = TARGET_RAMP_TICKS_S2 * dt;
        if (ramped_ < target) { ramped_ += step; if (ramped_ > target) ramped_ = target; }
        else                  { ramped_ -= step; if (ramped_ < target) ramped_ = target; }

        frac_ += ramped_ * dt;               // estimated 1x ticks travelled
        long whole = (long)frac_;
        whole_ += whole;
        frac_ -= whole;

        if (fabs(ramped_) < OPEN_STOP_TICKS_S) { dir_ = 0; return DriveCmd{DRIVE_COAST, 0}; }
        int8_t dir = (ramped_ > 0) ? 1 : -1;
        if (dir != dir_) { dir_ = dir; kickEnd_ = ms + KICK_MS; }

        float a = fabs(ramped_);
        float pwm = (dir > 0) ? (p_.ffBaseF + p_.ffKF * a) * gainF_ : (p_.ffBaseR + p_.ffKR * a) * gainR_;
        float flr = (dir > 0) ? p_.floorF : p_.floorR;
        float kick = (dir > 0) ? p_.kickF : p_.kickR;
        if (pwm < flr) pwm = flr;
        if ((long)(ms - kickEnd_) < 0 && pwm < kick) pwm = kick;
        if (pwm > 255.0f) pwm = 255.0f;
        return DriveCmd{DRIVE_RUN, dir * (int)(pwm + 0.5f)};
    }

    // Stop now (watchdog, fault, 's'). The estimate is kept.
    void reset() { ramped_ = 0.0f; dir_ = 0; }

    // Estimated position in 2x counts, the unit EncoderDriver::count() uses.
    long counts2x() const { return whole_ * 2; }

private:
    WheelParams p_;
    float gainF_, gainR_;
    float ramped_ = 0.0f, frac_ = 0.0f;
    long whole_ = 0;
    int8_t dir_ = 0;
    unsigned long kickEnd_ = 0;
};
