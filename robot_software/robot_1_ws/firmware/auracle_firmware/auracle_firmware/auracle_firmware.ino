#include <avr/wdt.h>
#include "config.h"
#include "motor_driver.h"
#include "encoder_driver.h"
#include "pid_controller.h"
#include "imu_driver.h"
#include "serial_protocol.h"

// Clear the WDT flag right after reset, or older bootloaders reset-loop.
uint8_t mcusr_mirror __attribute__((section(".noinit")));
void get_mcusr(void) __attribute__((naked, used, section(".init3")));
void get_mcusr(void) {
    mcusr_mirror = MCUSR;
    MCUSR = 0;
    wdt_disable();
}

// If the faster wheel is commanded below this, the Pi is asking for a stop.
// Anything above it keeps both wheels turning (see conditionTargets).


// MODE_PID with zero targets is "stopped": the controllers brake (if still rolling) and coast.
enum Mode : uint8_t { MODE_PID, MODE_OPEN_LOOP, MODE_RAW };   // MODE_RAW: 'p'/'q' test commands

const WheelParams LB_PARAMS = {LB_KICK_FWD, LB_KICK_REV, LB_FLOOR_FWD, LB_FLOOR_REV, LB_KP, LB_KI,
                               LB_FF_FWD_BASE, LB_FF_FWD_K, LB_FF_REV_BASE, LB_FF_REV_K};
const WheelParams RB_PARAMS = {RB_KICK_FWD, RB_KICK_REV, RB_FLOOR_FWD, RB_FLOOR_REV, RB_KP, RB_KI,
                               RB_FF_FWD_BASE, RB_FF_FWD_K, RB_FF_REV_BASE, RB_FF_REV_K};

const MotorDriver::FrontParams LF_PARAMS = {LF_OFFSET_FWD, LF_OFFSET_REV, LF_KICK_FRONT_FWD, LF_KICK_FRONT_REV, LF_MIN_FWD, LF_MIN_REV};
const MotorDriver::FrontParams RF_PARAMS = {RF_OFFSET_FWD, RF_OFFSET_REV, RF_KICK_FRONT_FWD, RF_KICK_FRONT_REV, RF_MIN_FWD, RF_MIN_REV};
MotorDriver leftSide(PIN_L_FWD, PIN_L_REV, PIN_LB_EN, PIN_LF_EN, LF_PARAMS);
MotorDriver rightSide(PIN_R_FWD, PIN_R_REV, PIN_RB_EN, PIN_RF_EN, RF_PARAMS);
EncoderDriver leftEnc(PIN_LB_ENC_INT, PIN_LB_ENC_DIR, LB_ENC_SIGN);
EncoderDriver rightEnc(PIN_RB_ENC_INT, PIN_RB_ENC_DIR, RB_ENC_SIGN);
PIDController leftPid(LB_PARAMS), rightPid(RB_PARAMS);
OpenLoopWheel rightOpen(RB_PARAMS, RB_OPEN_GAIN_FWD, RB_OPEN_GAIN_REV);   // used when !RB_ENCODER_OK
IMUDriver imu;
SerialProtocol link;

Mode mode = MODE_PID;
bool faultLatched = false;
unsigned long faultMs = 0;       // when the fault latched
uint8_t faultRetries = 0;        // auto-restarts since the last zero command
float targetL = 0, targetR = 0;  // 1x ticks/s
int pwmL = 0, pwmR = 0;          // open loop
int rawLB = 0, rawLF = 0, rawRB = 0, rawRF = 0;   // MODE_RAW, per wheel
unsigned long lastCmdMs = 0, lastCtrlMs = 0, lastSampleUs = 0;
long lastCountL = 0, lastCountR = 0;
int16_t imuRaw[6] = {0, 0, 0, 0, 0, 0};

void isrLeft() { leftEnc.tick(); }
void isrRight() { rightEnc.tick(); }
ISR(PCINT2_vect) { rightEnc.tickB(); }   // D4 = PD4 = PCINT20, only enabled when RB_COUNT_ON_B

// Conditions the left/right pair TOGETHER so a moving robot never has a stationary side
// (a stopped wheel means brake -> coast -> kick restart, which is the vibration and jumping).
// Rule: the yaw demand (r - l) is kept exact, forward speed absorbs the correction.
//   - both wheels near zero            -> real stop
//   - pure spin request (l ~= -r)      -> stays a spin in place, each wheel clamped to MIN..MAX
//   - difference > MAX - MIN           -> counter-rotate (pivot), no wheel can run slower than MIN
//   - otherwise (arc)                  -> shift both wheels together until the slower is at MIN
// Works in wheel rev/s, not ticks: the two sides have different ticks/rev, so shifting both by
// the same number of ticks would add a yaw the Pi never asked for (the robot drifts in an arc).
// The arc/spin switch has hysteresis so a turn held near the boundary doesn't keep reversing
// the inner side (each reversal = brake, wait for standstill, kick: the stop-start shudder).
void conditionTargets(float& l, float& r) {
    static bool spinning = false;
    if (max(fabs(l), fabs(r)) < CMD_DEADBAND_TICKS_S) { l = r = 0; spinning = false; return; }

    // Strictest limits that both sides can meet, in rev/s.
    const float minRev = max(MIN_TARGET_TICKS_S / LB_TICKS_PER_REV, MIN_TARGET_TICKS_S / RB_TICKS_PER_REV);
    // Forward is capped lower because RB is weak forward (RB_FWD_MAX_TICKS_S).
    const float maxRevRev = min(MAX_TICKS_S / LB_TICKS_PER_REV, MAX_TICKS_S / RB_TICKS_PER_REV);
    const float maxRevFwd = min(maxRevRev, RB_FWD_MAX_TICKS_S / RB_TICKS_PER_REV);

    float lr = l / LB_TICKS_PER_REV, rr = r / RB_TICKS_PER_REV;
    float d = rr - lr;                     // yaw demand
    float m = 0.5f * (lr + rr);            // forward demand
    float dir = (m >= 0) ? 1.0f : -1.0f;
    // Inner wheel speed in the direction of travel, relative to forward demand:
    // 1 = straight, 0 = inner wheel stopped, < 0 = the caller wants the inner wheel reversed.
    float q = (fabs(m) < 0.002f) ? -1.0f : min(dir * lr, dir * rr) / fabs(m);

    // Pivot only when the inner wheel is really asked to run backwards (a turn-on-the-spot command).
    // A sharp-but-forward arc used to become a pivot as soon as it exceeded what the wheels can arc,
    // and Nav2's small steering corrections then flipped arc <-> pivot: the "aggressive" turning.
    if (spinning) { if (q > 0.0f) spinning = false; }
    else          { if (q < -0.3f) spinning = true; }

    if (spinning) {                        // counter-rotate
        float s = (d >= 0) ? 1.0f : -1.0f;        // +1: right wheel forward
        float h = constrain(fabs(d) * 0.5f, minRev, (s > 0) ? maxRevFwd : maxRevRev);
        l = -s * h * LB_TICKS_PER_REV;
        r =  s * h * RB_TICKS_PER_REV;
        return;
    }

    // Arc: both wheels turn the same way. Lift both so the slower one reaches minRev, cap the faster
    // at maxRev; if the asked-for turn is sharper than that range allows, drive the tightest arc
    // (slow wheel at minRev, fast wheel at maxRev) instead of pivoting.
    const float maxRev = (dir > 0) ? maxRevFwd : maxRevRev;
    float a = dir * lr, b = dir * rr;      // work in the direction of travel
    float lo = min(a, b);
    if (lo < minRev) { float s = minRev - lo; a += s; b += s; }
    float hi = max(a, b);
    if (hi > maxRev) { float s = hi - maxRev; a -= s; b -= s; }
    if (a < minRev) a = minRev;
    if (b < minRev) b = minRev;
    l = dir * a * LB_TICKS_PER_REV;
    r = dir * b * RB_TICKS_PER_REV;
}

// Zero the targets and cut the motors now. A wheel that is still rolling gets a short brake
// from the controller on the next control period, then coasts.
void stopAll() {
    mode = MODE_PID;
    targetL = targetR = 0;
    pwmL = pwmR = 0;
    rawLB = rawLF = rawRB = rawRF = 0;
    leftSide.coast();
    rightSide.coast();
    rightOpen.reset();
}

void apply(MotorDriver& side, const DriveCmd& c) {
    switch (c.mode) {
        case DRIVE_RUN:   side.drive(c.effort); break;
        case DRIVE_BRAKE: side.brake(); break;
        default:          side.coast(); break;
    }
}

// A latched fault stops everything until the Pi sends 's' or a zero velocity command.
float faultTargetL = 0, faultTargetR = 0;   // targets at the moment of the fault (for the report)
void checkFaults() {
    if (REPORT_FAULTS) {   // stalls that were only reported, not stopped (FAULT_STOP_ON_STALL false)
        Fault rl = leftPid.takeReport(), rr = rightPid.takeReport();
        if (rl != FAULT_NONE) link.sendFault('L', rl, (int)leftPid.speed(), (int)targetL);
        if (rr != FAULT_NONE) link.sendFault('R', rr, (int)rightPid.speed(), (int)targetR);
    }
    Fault fl = leftPid.fault(), fr = rightPid.fault();
    if (fl == FAULT_NONE && fr == FAULT_NONE) return;
    faultTargetL = targetL;
    faultTargetR = targetR;
    faultLatched = true;
    faultMs = millis();
    stopAll();   // zeroes targetL/R, so grab the commanded values first
    if (REPORT_FAULTS) {
        if (fl != FAULT_NONE) link.sendFault('L', fl, (int)leftPid.speed(), (int)faultTargetL);
        if (fr != FAULT_NONE) link.sendFault('R', fr, (int)rightPid.speed(), (int)faultTargetR);
    }
}

void handle(const Command& cmd) {
    switch (cmd.type) {
        case 'v': {
            float tl = cmd.a, tr = cmd.b;
            conditionTargets(tl, tr);
            if (tl == 0 && tr == 0) faultRetries = 0;    // a real stop request re-arms the retries
            if (faultLatched) {
                bool retry = (tl != 0 || tr != 0) && faultRetries < FAULT_MAX_RETRIES &&
                             millis() - faultMs >= FAULT_RETRY_MS;
                if ((tl != 0 || tr != 0) && !retry) break;   // stay stopped (cooling down / out of retries)
                if (retry) faultRetries++;
                faultLatched = false;            // zero command or retry: clear and restart from rest
                leftPid.clearFault();
                rightPid.clearFault();
                leftPid.reset();
                rightPid.reset();
            }
            lastCmdMs = millis();
            targetL = tl;
            targetR = tr;
            if (mode != MODE_PID) {
                leftPid.reset();
                rightPid.reset();
                mode = MODE_PID;
            }
            break;
        }
        case 'o':
            if (faultLatched) break;
            lastCmdMs = millis();
            pwmL = constrain((int)cmd.a, -MAX_PWM, MAX_PWM);
            pwmR = constrain((int)cmd.b, -MAX_PWM, MAX_PWM);
            targetL = targetR = 0;
            if (mode != MODE_OPEN_LOOP) {
                leftPid.reset();
                rightPid.reset();
                mode = MODE_OPEN_LOOP;
            }
            break;
        // Test only: p <LB> <LF> / q <RB> <RF> = raw signed PWM per wheel, used to find each motor's
        // start/stop PWM (front_offset_test.py). Same watchdog as 'o'.
        case 'p':
        case 'q':
            if (faultLatched) break;
            lastCmdMs = millis();
            if (cmd.type == 'p') { rawLB = constrain((int)cmd.a, -MAX_PWM, MAX_PWM); rawLF = constrain((int)cmd.b, -MAX_PWM, MAX_PWM); }
            else                 { rawRB = constrain((int)cmd.a, -MAX_PWM, MAX_PWM); rawRF = constrain((int)cmd.b, -MAX_PWM, MAX_PWM); }
            targetL = targetR = 0;
            if (mode != MODE_RAW) {
                leftPid.reset();
                rightPid.reset();
                mode = MODE_RAW;
            }
            break;
        // Test only: g <kp> <ki> / h <kp> <ki> = live left / right PI gains (pid_tune.py).
        case 'g': leftPid.setGains(cmd.a, cmd.b); break;
        case 'h': rightPid.setGains(cmd.a, cmd.b); break;
        case 's':
            faultLatched = false;
            faultRetries = 0;
            leftPid.clearFault();
            rightPid.clearFault();
            stopAll();
            break;
    }
}

void setup() {
    MotorDriver::configurePwmTimers();  // Timer1 -> ~976 Hz, before any analogWrite
    leftSide.begin();
    rightSide.begin();
    link.begin();

    leftEnc.begin();
    rightEnc.begin();
    attachInterrupt(digitalPinToInterrupt(PIN_LB_ENC_INT), isrLeft, CHANGE);   // 2x decoding
    if (RB_COUNT_ON_B) {
        static_assert(PIN_RB_ENC_DIR == 4, "PCINT setup below assumes the right B wire is on D4");
        PCMSK2 |= _BV(PCINT20);
        PCICR |= _BV(PCIE2);
    } else {
        attachInterrupt(digitalPinToInterrupt(PIN_RB_ENC_INT), isrRight, CHANGE);
    }

    if (IMU_ENABLED) {
        imu.begin();
        imu.calibrateGyro(200);  // ~0.5 s, robot must be still
    }

    wdt_enable(WDTO_2S);  // after calibration so it can't trip it
    lastCmdMs = lastCtrlMs = millis();
    lastSampleUs = micros();
    link.sendReady();
}

void loop() {
    wdt_reset();

    Command cmd;
    while (link.poll(cmd)) handle(cmd);

    unsigned long now = millis();
    bool driving = (mode != MODE_PID) || targetL != 0 || targetR != 0;
    if (driving && now - lastCmdMs > COMMS_TIMEOUT_MS) stopAll();

    if (now - lastCtrlMs < CONTROL_PERIOD_MS) return;
    lastCtrlMs += CONTROL_PERIOD_MS;
    if (now - lastCtrlMs > CONTROL_PERIOD_MS) lastCtrlMs = now;  // fell behind - don't burst

    unsigned long nowUs = micros();
    float dt = (nowUs - lastSampleUs) * 1e-6f;
    lastSampleUs = nowUs;

    rightEnc.setDirection(rightSide.direction());   // used by B-only counting
    long countL = leftEnc.count();   // 2x counts
    long countR = rightEnc.count();
    float rawL = (countL - lastCountL) * COUNT_TO_TICK / dt;   // 1x ticks/s
    float rawR = (countR - lastCountR) * COUNT_TO_TICK / dt;
    lastCountL = countL;
    lastCountR = countR;

    if (mode == MODE_PID) {
        // Front wheels get extra PWM in proportion to how hard the robot is turning.
        float turn = constrain(fabs(targetR - targetL) / BOOST_FULL_DIFF_TICKS_S, 0.0f, 1.0f);
        uint8_t boost = (uint8_t)(FRONT_TURN_BOOST_PWM * turn + 0.5f);
        leftSide.setFrontBoost(boost);
        rightSide.setFrontBoost(boost);
        leftPid.setBoost(REAR_TURN_BOOST_PWM * turn);
        rightPid.setBoost(REAR_TURN_BOOST_PWM * turn);
        apply(leftSide, leftPid.update(targetL, rawL, dt, now));
        if (RB_ENCODER_OK) apply(rightSide, rightPid.update(targetR, rawR, dt, now));
        else               apply(rightSide, rightOpen.update(faultLatched ? 0.0f : targetR, dt, now));
        if (!faultLatched) checkFaults();
    } else {
        leftSide.setFrontBoost(0);
        rightSide.setFrontBoost(0);
        leftPid.observe(rawL);
        rightPid.observe(rawR);
        if (mode == MODE_RAW) {
            leftSide.driveRaw(rawLB, rawLF);
            rightSide.driveRaw(rawRB, rawRF);
        } else {
            leftSide.drive(pwmL);
            rightSide.drive(pwmR);
        }
    }

    if (IMU_ENABLED) imu.read(imuRaw);
    // No right encoder: report the open-loop position estimate instead, so the Pi's odometry
    // follows what the right side was told to do. Open-loop test mode ('o') still sends the
    // real encoder count, so the bench test keeps showing what the encoder sees.
    if (!RB_ENCODER_OK && mode == MODE_PID) countR = rightOpen.counts2x();
    if (TELEMETRY_RAW_2X) link.sendTelemetry(now, countL, countR, imuRaw);
    else                  link.sendTelemetry(now, countL >> 1, countR >> 1, imuRaw);   // 1x-equivalent
}