#include <Arduino.h>
#include "config.h"
#include "motor_driver.h"
#include "encoder_driver.h"
#include "pid_controller.h"
#include "serial_protocol.h"
#include "watchdog.h"

/*
================================================
 PID LOOP
================================================
*/
unsigned long lastPIDUpdate = 0;

/*
================================================
 SETUP
================================================
*/
void setup()
{
    /*
      Start serial communication
    */
    serialBegin();

    /*
      Initialize L298 drivers
    */
    motorBegin();

    /*
      Initialize encoders
    */
    encoders.begin();

    /*
      Initial PID tuning
      These are starting values.
      They will be tuned later.
    */
    leftPID.setTunings(2.0, 0.0, 0.2);
    rightPID.setTunings(2.0, 0.0, 0.2);

    /*
      Start watchdog timer
    */
    watchdogReset();

    // REMOVED: Serial.println("Robot2 Firmware Ready");
    // This keeps the serial buffer 100% clean of text strings for ROS 2.
}

/*
================================================
 MAIN LOOP
================================================
*/
void loop()
{
    /*
      Always listen for commands from Serial
    */
    serialUpdate();

    /*
      PID update loop (20Hz)
      Only runs in MODE_PID. In MODE_OPEN_LOOP, motorSetPWM()
      was already called directly by the 'o' command handler,
      so we must NOT overwrite it here every 50ms.
    */
    if (getControlMode() == MODE_PID && millis() - lastPIDUpdate >= PID_PERIOD)
    {
        lastPIDUpdate = millis();

        /*
          Update encoder differences
          Gives ticks travelled since last PID cycle
        */
        encoders.update();

        long leftTicks = encoders.getLeftDelta();
        long rightTicks = encoders.getRightDelta();

        /*
          Compute PID output
        */
        float leftPWM = leftPID.update((float)leftTicks);
        float rightPWM = rightPID.update((float)rightTicks);

        /*
          Send PWM to motors
        */
        motorSetPWM((int)leftPWM, (int)rightPWM);
    }

    /*
      Safety watchdog
      If Raspberry Pi stops sending motion commands ('m' or 'o'),
      stop robot. Note: querying encoders ('e') does NOT reset this
      timer, so a dead teleop link is caught even if odometry
      polling keeps running.
    */
    if (watchdogExpired())
    {
        motorSetPWM(0, 0);
        leftPID.reset();
        rightPID.reset();
    }
}
