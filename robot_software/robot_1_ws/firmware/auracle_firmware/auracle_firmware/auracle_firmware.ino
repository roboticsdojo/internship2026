#include <avr/wdt.h>
#include "config.h"
#include "motor_driver.h"
#include "encoder_driver.h"
#include "pid_controller.h"
#include "imu_driver.h"
#include "serial_protocol.h"
#include "watchdog.h"

// Runs before global constructors / main(), as early as possible after
// reset. Some Nano bootloaders don't clear the "watchdog caused this reset"
// flag, so if the WDT is left enabled across a watchdog-triggered reset,
// the chip can boot-loop forever before setup() ever gets a chance to
// re-arm it deliberately. Clearing MCUSR and disabling the WDT here breaks
// that loop; watchdog.begin() in setup() re-enables it with the real timeout.
uint8_t mcusr_mirror __attribute__((section(".noinit")));
void get_mcusr(void) __attribute__((naked, used, section(".init3")));
void get_mcusr(void)
{
    mcusr_mirror = MCUSR;
    MCUSR = 0;
    wdt_disable();
}

// Instantiate objects
MotorDriver leftMotor(L_MOTOR_PWM, L_MOTOR_IN1, L_MOTOR_IN2);
MotorDriver rightMotor(R_MOTOR_PWM, R_MOTOR_IN1, R_MOTOR_IN2);
EncoderDriver leftEnc(L_ENC_A, L_ENC_B);
EncoderDriver rightEnc(R_ENC_A, R_ENC_B);
PIDController leftPID(PID_KP, PID_KI, PID_KD);
PIDController rightPID(PID_KP, PID_KI, PID_KD);
IMUDriver imu;
SerialProtocol protocol;
Watchdog watchdog;

bool imu_ok = false;
unsigned long lastRosPacketTime = 0;
unsigned long lastLoopTime = 0;

// Target wheel speed in encoder ticks/sec, set from the last parsed "v"
// command (converted from the rad/s the hardware interface sends).
double targetTicksL = 0, targetTicksR = 0;

// Link ISRs
EncoderDriver* EncoderDriver::instanceL = &leftEnc;
EncoderDriver* EncoderDriver::instanceR = &rightEnc;

void setup() {
    Serial.begin(BAUDRATE);
    watchdog.begin();
    imu_ok = imu.begin();
    if (!imu_ok) {
        // Don't halt - the robot can still drive without IMU data, and the
        // hardware interface will just receive stale/zero IMU state. But
        // make it visible on the serial monitor for debugging.
        Serial.println("WARN imu_init_failed");
    }
    attachInterrupt(digitalPinToInterrupt(L_ENC_A), EncoderDriver::isrL, RISING);
    attachInterrupt(digitalPinToInterrupt(R_ENC_A), EncoderDriver::isrR, RISING);

    lastLoopTime = millis();
    lastRosPacketTime = millis();

    // Lets auracle_hardware's ArduinoComms::waitForReady() know we're
    // actually up (rather than the host racing the Nano's post-upload
    // auto-reset and reading garbage/nothing for the first ~1-2s).
    protocol.sendReady();
}

void loop() {
    // 1. Pet the hardware watchdog immediately
    watchdog.pet();

    unsigned long now = millis();
    double dt = (now - lastLoopTime) / 1000.0;
    if (dt <= 0.0) {
        dt = 0.001;  // guard against millis() rollover / same-ms calls
    }
    lastLoopTime = now;

    // 2. Non-blocking parse of any velocity command that has arrived.
    double targetRadL = 0, targetRadR = 0;
    if (protocol.parseVelocity(targetRadL, targetRadR)) {
        lastRosPacketTime = now;
        targetTicksL = RAD_S_TO_TICKS_S(targetRadL);
        targetTicksR = RAD_S_TO_TICKS_S(targetRadR);
    }

    // 3. Communication Safety Check
    if (!watchdog.isCommsAlive(lastRosPacketTime)) {
        // EMERGENCY STOP: ROS is gone, stop the motors!
        targetTicksL = 0;
        targetTicksR = 0;
        leftPID.reset();
        rightPID.reset();
    }

    // Current speed in ticks/sec, matching the setpoint's units, computed
    // from the change in raw encoder count over this loop's dt (rather than
    // the old fixed "count / 100.0" placeholder, which wasn't a speed at
    // all - it was a scaled position).
    static long lastCountL = 0, lastCountR = 0;
    long countL = leftEnc.getCount();
    long countR = rightEnc.getCount();
    double curTicksL = (countL - lastCountL) / dt;
    double curTicksR = (countR - lastCountR) / dt;
    lastCountL = countL;
    lastCountR = countR;

    leftMotor.setSpeed(leftPID.compute(targetTicksL, curTicksL, dt));
    rightMotor.setSpeed(rightPID.compute(targetTicksR, curTicksR, dt));

    // Telemetry
    float acc[3] = {0, 0, 0};
    float gyro[3] = {0, 0, 0};
    if (imu_ok) {
        imu.readData(acc, gyro);
    }
    protocol.sendTelemetry(countL, countR, acc, gyro);

    delay(33); // ~30Hz loop, matches controller_manager's update_rate in my_controllers.yaml
}
