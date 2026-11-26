#pragma once
#include <Arduino.h>

// ============================================================
// Every PWM constant below was measured with ALL of:
//   - Timer1 (D9/D10) switched to ~976 Hz by MotorDriver::configurePwmTimers()
//     so all four enables run at the same frequency (Timer0 = D5/D6 already does)
//   - 2x encoder decoding (CHANGE on channel A, channel B = direction)
//   - robot propped up (no load)  -> re-check on the ground
// Change any of those and the PWM numbers are no longer valid.
// ============================================================

// --- Pins (README "Real robot") ---
// Direction bridged per side (IN1+IN3 / IN2+IN4), PWM per wheel.
// No Servo library: it takes Timer1 and kills PWM on D9/D10.
constexpr uint8_t PIN_RF_EN = 5;
constexpr uint8_t PIN_RB_EN = 6;
constexpr uint8_t PIN_R_FWD = 7;
constexpr uint8_t PIN_R_REV = 8;
constexpr uint8_t PIN_LB_EN = 9;
constexpr uint8_t PIN_LF_EN = 10;
constexpr uint8_t PIN_L_FWD = A0;
constexpr uint8_t PIN_L_REV = A1;
// Rear encoders, 2x decoding: CHANGE interrupt on the INT pin (channel A),
// DIR pin = channel B. A == B counts as ENC_SIGN, A != B as -ENC_SIGN.
constexpr uint8_t PIN_RB_ENC_INT = 2;
constexpr uint8_t PIN_RB_ENC_DIR = 4;
constexpr uint8_t PIN_LB_ENC_INT = 3;
constexpr uint8_t PIN_LB_ENC_DIR = 12;  // moved off D13 (onboard LED loads the pull-up)
// MPU6050 on SOFTWARE I2C: A4/A5 hardware I2C is unusable (the Nano's A5 pin is damaged: it cannot be pulled LOW).
constexpr uint8_t PIN_IMU_SCL = 11;   // D11. Pin scan 2026-10-25: A2 pull-up reads 0, A3 and A5 stuck HIGH (damaged)
constexpr uint8_t PIN_IMU_SDA = A4;   // A4 tested ok; A5 (hardware SCL) is damaged, so Wire is unusable
// false = never touch the IMU pins (telemetry IMU fields stay 0). OFF until the A2/A3 wiring is
// confirmed: a pin probe found A3 held hard at 5 V, and driving a line LOW that is wired to VCC
// shorts it and can kill the pin.
constexpr bool IMU_ENABLED = true;   // IMU on A4 (SDA) / D11 (SCL) answers at 0x68, WHO_AM_I 0x72 (2026-10-25)

constexpr long BAUDRATE = 500000;  // was 115200. Exact on a 16 MHz AVR. Must match BAUD_500000 in arduino_comms.hpp

// --- Encoders ---
// Measured: A == B happens when driving BACKWARD on both rear wheels, so the sign is -1.
constexpr int8_t LB_ENC_SIGN = -1;
constexpr int8_t RB_ENC_SIGN = -1;
// 2x decoding gives 2 counts per old 1x tick. All speeds/targets stay in the old 1x
// "ticks/s", so the PI constants and the Pi side don't change.
constexpr float COUNT_TO_TICK = 0.5f;
// 1x ticks per wheel revolution. MUST match left/right_enc_counts_per_rev in ros2_control.xacro.
// conditionTargets() mixes the two sides in rev/s with these, so equal wheel speeds really are equal.
constexpr float LB_TICKS_PER_REV = 985.0f;   // hand-counted 2026-10-23 (9847/10) and 2026-10-25 (9873/10)
constexpr float RB_TICKS_PER_REV = 990.0f;   // re-counted 2026-10-25 with A+B: 10 fwd = 9880, 2 back = -2002 (942 on 10-23 was A dropping pulses)
// Right rear encoder: its signal drops out (D2 counts nothing while the wheel turns, 2026-10-23).
// false = the right side runs open loop on the RB feedforward below (no PI, no fault checks) and
// telemetry reports its commanded position instead of encoder counts. Set true once it's fixed.
constexpr bool RB_ENCODER_OK = true;
// Open-loop right-side PWM multipliers. Straight-line test on the ground: if the robot veers RIGHT
// going forward, raise RB_OPEN_GAIN_FWD (e.g. 1.05, 1.10); veers left, lower it. Same for reverse.
constexpr float RB_OPEN_GAIN_FWD = 1.0f;
constexpr float RB_OPEN_GAIN_REV = 1.0f;
constexpr float OPEN_STOP_TICKS_S = 150.0f;   // ramped right target below this = coast
// Right rear encoder: channel A (D2) drops pulses, channel B (D4) is clean (probe, 2026-10-23).
// true = count the right wheel on B edges only (pin-change interrupt), direction from the drive
// command. Set false once the A wire is fixed to go back to normal quadrature decoding.
constexpr bool RB_COUNT_ON_B = true;   // A wire still drops out while driving: trial 3 had 22 NO_MOTION reports, RB ~0, lined up with SLAM jumps (2026-11-04). B-only 992/rev vs A+B 990
// false: telemetry sends 1x-equivalent counts (nothing changes on the Pi).
// true : telemetry sends raw 2x counts (then double the encoder counts-per-rev in ros2_control.xacro).
constexpr bool TELEMETRY_RAW_2X = false;

// --- Motors ---
constexpr int MAX_PWM = 255;

// KICK  = PWM held from rest (until the wheel passes KICK_EXIT_TICKS_S or KICK_MS runs out)
//         so the wheel beats static friction. Worst breakaway seen + margin.
//         RB breakaway moved between 81 and 129 over repeated runs (stiction depends on wheel position).
// FLOOR = lowest PWM the controller may command while turning. Kept ~15% below the PWM needed at
//         MIN_TARGET_TICKS_S. PROVISIONAL until the 'h' (hold-min) test is run at 976 Hz.
constexpr uint8_t LB_KICK_FWD  = 64,  LB_KICK_REV  = 69;   // start PWM x1.15 (2026-10-25: 56 / 60)
constexpr uint8_t RB_KICK_FWD  = 64,  RB_KICK_REV  = 55;   // start PWM x1.15 (2026-10-25: 56 / 48). Was 118/135 while RB was miswired
constexpr uint8_t LB_FLOOR_FWD = 70,  LB_FLOOR_REV = 70;   // all wheels stop near PWM 30 (2026-10-25)
constexpr uint8_t RB_FLOOR_FWD = 70,  RB_FLOOR_REV = 70;

// Feedforward: PWM = BASE + K * |ramped target ticks/s|. Least-squares fits of settled
// steady-state means (speed -> PWM) from the 976 Hz auto test, no load:
//   LB fwd  600->102  1013->156  1204->195   (+600 coming down ->97)
//   LB rev  604->98    800->122  1009->156
//   RB fwd  603->108  1006->165  1204->206
//   RB rev  807->117  1003->152                (only 2 settled points: the -600 row was lost to a bad wire,
//                                               so this line is LB rev scaled by 0.965, which fits both)
// Fit residuals are +-5 PWM; the PI trims the rest.
constexpr float LB_FF_FWD_BASE = 17.3f, LB_FF_FWD_K = 0.1590f;   // ff_sweep 2026-10-25 (976 Hz, 40 Hz loop, in air)
constexpr float LB_FF_REV_BASE = 14.7f, LB_FF_REV_K = 0.1585f;
// RB forward re-measured 2026-10-23 (wheels up, 976 Hz): 120->290, 180->488 ticks/s. RB has lost
// most of its forward strength (reverse is unchanged: -120 -> -806), likely one half of its L298
// channel. Old fit was BASE 9.7, K 0.1599.
constexpr float RB_FF_FWD_BASE = 14.1f, RB_FF_FWD_K = 0.1647f;   // RB healthy again after the rewiring (was 32.1 + 0.303)
constexpr float RB_FF_REV_BASE = 14.9f, RB_FF_REV_K = 0.1565f;

// Front wheels (no encoders) follow their side's REAR PWM with a fixed OFFSET (friction is a roughly
// fixed extra PWM, not a percentage):  front = rear + OFFSET (+ turn boost), never below MIN while
// driving, and at least KICK for FRONT_KICK_MS after starting from rest.
// Measured 2026-10-25, wheels in the air, 976 Hz (front_offset_test.py), start / stop PWM:
//   LF fwd 96/54  rev 214/132   (old damaged motor, replaced 2026-11-04)
//   RF fwd 56/32  rev 58/28     LB fwd 56/30  rev 60/34     RB fwd 56/26  rev 48/30
// OFFSET = front stop - rear stop. KICK = front start x ~1.1. MIN = front stop x ~1.1.
// Straight-line test (floor): drifts toward the LEFT -> raise LF_OFFSET_FWD; toward the right -> lower it.
constexpr int16_t LF_OFFSET_FWD = 0,   LF_OFFSET_REV = 15;   // LF motor replaced 2026-11-04. Rev: stalled in slow left spins at +8
constexpr int16_t RF_OFFSET_FWD = -10, RF_OFFSET_REV = 40;   // rev: stalls in right spins below +40. fwd: too fast in left spins at +6 (2026-11-04)
constexpr uint8_t LF_KICK_FRONT_FWD = 80,  LF_KICK_FRONT_REV = 80;   // provisional, new LF. Was 255/255 for the damaged motor
constexpr uint8_t RF_KICK_FRONT_FWD = 64,  RF_KICK_FRONT_REV = 67;
constexpr uint8_t LF_MIN_FWD = 36, LF_MIN_REV = 32;   // provisional, new LF (RF's values). Was 230/255 for the damaged motor
constexpr uint8_t RF_MIN_FWD = 36, RF_MIN_REV = 32;
constexpr unsigned long FRONT_KICK_MS = 400;   // front kick from rest (not slowed by the soft start)

// --- Speed targets (1x ticks/s) ---
// MIN: lowest speed validated for both rears (600 settled within 1% on both). Anything lower is a stop.
//      Lower it only after the 'h' test and a 'run t300 / t450' check in the test sketch.
// MAX: validated at 1200 with 20% PWM headroom (LB 195, RB 206 of 255). RB reaches ~1540 at PWM 255.
constexpr float MIN_TARGET_TICKS_S = 450.0f;   // was 600; lowered so arcs fit under RB's weak forward cap
constexpr float MAX_TICKS_S        = 1200.0f;
// conditionTargets() caps the right wheel's forward speed at this. Raise it once RB is repaired.
constexpr float RB_FWD_MAX_TICKS_S = 1200.0f;   // RB healthy again (2026-10-25): cap = MAX_TICKS_S. Was 650/740 while miswired

// Same-sign wheel pairs can differ by at most MAX - MIN. Beyond that, counter-rotate.
constexpr float CMD_DEADBAND_TICKS_S = 150.0f;   // faster wheel below this = stop

// --- Rear wheel controller: PI + feedforward (tuned in the auto test) ---
constexpr float LB_KP = 0.045f;   // pid_tune sweeps 2026-10-25 (in air, 40 Hz). Was 0.03
constexpr float LB_KI = 0.30f;    // PWM per second per (tick/s of error). Was 0.15: left settled 6-8% off
constexpr float RB_KP = 0.0375f;  // was 0.025
constexpr float RB_KI = 0.40f;    // was 0.20
constexpr unsigned long CONTROL_PERIOD_MS = 25;   // was 50: 40 Hz loop + telemetry (needs the 500000 baud link)
constexpr float SPEED_FILTER_ALPHA    = 0.3f;     // speed += alpha * (raw - speed). Was 0.4 at 50 ms; half the ticks per sample now
constexpr float ERR_CLAMP_TICKS_S     = 300.0f;   // limits the push during spin-up
constexpr float INTEG_MAX_PWM         = 100.0f;   // integral clamp (anti-windup, +/-). Was 60: too little to un-stall a spin on the floor
constexpr float MAX_PWM_STEP          = 12.0f;    // max PWM change per control period (25 ms now: 2x the old slew rate)
// Soft start in MotorDriver::drive(): max PWM rise per control period, for every motor (rear and
// front). 0 -> 135 kick in 4 periods (200 ms) instead of one step. Limits the start-up current
// spike that browns out the Pi when it runs off the same LiPo. Decreases are not limited.
constexpr uint8_t PWM_RISE_PER_CALL   = 18;   // was 35 per 50 ms; same ~700 PWM/s at 25 ms
constexpr float TARGET_RAMP_TICKS_S2  = 4000.0f;  // setpoint ramp (ticks/s per second). Was 2000 (~0.42 m/s^2), slower than diff_cont
constexpr float OVERSPEED_COAST_RATIO = 1.4f;    // above this x target: cut PWM to 0. 1.25 caused bursts in spins; at 1.6 wheels reached 1.75x target on grass (2026-11-05)
constexpr unsigned long KICK_MS       = 350;    // was 450; the soft-start ramp is 2x faster at 25 ms
constexpr float KICK_EXIT_TICKS_S     = 150.0f;   // kick ends early once the wheel is this fast

// --- Stopping and reversing ---
// Stop / reverse = short brake (both L298N inputs HIGH, EN HIGH) for BRAKE_MS if the wheel is
// moving faster than BRAKE_MIN_TICKS_S, then coast until standstill before driving the other way.
// Set BRAKE_MS to 0 to coast only.
constexpr unsigned long BRAKE_MS         = 120;   // was 200
constexpr float         BRAKE_MIN_TICKS_S = 50.0f;
constexpr unsigned long REVERSE_WAIT_MS  = 500;   // max wait for standstill before reversing (was 800)
constexpr float         STILL_TICKS_S    = 30.0f;

// Extra front-wheel PWM while turning (fronts have no encoder, so they can't sense the scrub load).
constexpr uint8_t FRONT_TURN_BOOST_PWM      = 30;      // was 80 for the failed motors; with healthy ones, fronts ran ~3x target and bursted (2026-11-04)
// Same idea for the closed-loop REAR wheels: extra feedforward PWM while turning. A skid-steer
// spin on the floor needed more than the PI could add in its 1 s before NO_MOTION fired: all 54
// faults in the first teleop run were the right side stalling (speed 0) during a spin.
constexpr uint8_t REAR_TURN_BOOST_PWM       = 20;      // was 60
constexpr float   BOOST_FULL_DIFF_TICKS_S   = 600.0f;  // |right - left| target where the full boost applies
// --- Safety ---
constexpr unsigned long COMMS_TIMEOUT_MS = 500;   // stop if the Pi goes quiet
// A wheel that runs away or never reaches its target latches a fault: every motor stops and
// stays stopped until an 's' or a zero 'v' command. This is what an unplugged encoder looks like.
constexpr unsigned long FAULT_GRACE_MS   = 800;   // no fault checks this long after a start
constexpr float FAULT_WRONG_SIGN_TICKS_S = 100.0f; // moving this fast against the command ...
constexpr uint8_t FAULT_WRONG_SIGN_COUNT = 12;     // ... for this many control periods (300 ms at 25 ms)
constexpr uint8_t FAULT_SATURATED_COUNT  = 80;     // PWM at 255 and < 80% of target for 2 s
constexpr float   FAULT_NO_MOTION_FRAC   = 0.25f;  // wheel slower than this fraction of the ramped target ...
constexpr uint8_t FAULT_NO_MOTION_COUNT  = 40;     // ... for 1 s = dead encoder or blocked wheel
// A latched fault no longer needs a stick release to recover: if the Pi is still asking for motion
// FAULT_RETRY_MS after the fault, the fault is cleared and the wheel is restarted from rest (kick PWM
// again, which also beats a stiction stall). After FAULT_MAX_RETRIES in a row with no zero command in
// between it stays latched, so a really dead encoder can't keep driving a wheel forever.
constexpr unsigned long FAULT_RETRY_MS  = 300; 
constexpr uint8_t       FAULT_MAX_RETRIES = 3;
// What a stall (NO_MOTION / SATURATED) does. true = stop every motor and retry, as before.
// false = report it to the Pi and keep driving: stopping can't free a wheel that is stalling in a
// skid-steer spin, it only turned every stall into the stop-start "burst". WRONG_SIGN always stops.
constexpr bool FAULT_STOP_ON_STALL = false;
// Send "F <L|R> <code> <speed> <target>" lines on a fault (1 = wrong sign, 2 = saturated, 3 = no motion). Make sure the Pi-side
// parser ignores lines that are not 'e ...' / 'READY', or set this to false.
constexpr bool REPORT_FAULTS = true;