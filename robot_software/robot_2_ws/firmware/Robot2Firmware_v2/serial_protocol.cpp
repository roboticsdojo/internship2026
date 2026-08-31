#include "serial_protocol.h"
#include "encoder_driver.h"
#include "motor_driver.h"
#include "pid_controller.h"
#include "watchdog.h"

String inputString = "";
bool commandReady = false;

static ControlMode currentMode = MODE_PID;

ControlMode getControlMode()
{
    return currentMode;
}

void serialBegin()
{
    Serial.begin(SERIAL_BAUD);
    inputString.reserve(40);
}

/*
  Manually split "arg1 arg2 arg3" into up to 3 floats using
  String::toFloat(). Avoids sscanf("%f"), which is unreliable on
  AVR: the default avr-libc sscanf linked by the Arduino IDE does
  not include floating-point conversion support, so "%f" silently
  parses as 0.0 rather than failing loudly.
*/
void parseArgs(const String &args, float &a1, float &a2, float &a3)
{
    a1 = 0.0;
    a2 = 0.0;
    a3 = 0.0;

    int s1 = args.indexOf(' ');

    if (s1 == -1)
    {
        a1 = args.toFloat();
        return;
    }

    a1 = args.substring(0, s1).toFloat();

    int s2 = args.indexOf(' ', s1 + 1);

    if (s2 == -1)
    {
        a2 = args.substring(s1 + 1).toFloat();
        return;
    }

    a2 = args.substring(s1 + 1, s2).toFloat();
    a3 = args.substring(s2 + 1).toFloat();
}

void serialUpdate()
{
    while (Serial.available())
    {
        char c = Serial.read();

        // Check for carriage return or newline BEFORE adding the character
        // to the string. This stops whitespace/hidden formatting chars
        // from corrupting the argument parsing.
        if (c == '\r' || c == '\n')
        {
            if (inputString.length() > 0)
            {
                commandReady = true;
            }
        }
        else
        {
            inputString += c;

            // Safety guard: if a huge amount of data arrives with no
            // terminator (e.g. line-ending misconfigured upstream, or
            // noise on the line), drop it rather than growing forever.
            if (inputString.length() > 40)
            {
                inputString = "";
            }
        }

        if (commandReady)
        {
            char command = inputString.charAt(0);

            float a1 = 0.0;
            float a2 = 0.0;
            float a3 = 0.0;

            int firstSpace = inputString.indexOf(' ');

            if (firstSpace != -1)
            {
                String args = inputString.substring(firstSpace + 1);
                parseArgs(args, a1, a2, a3);
            }

            processCommand(command, a1, a2, a3);

            inputString = "";
            commandReady = false;
        }
    }
}

void processCommand(char command, float arg1, float arg2, float arg3)
{
    switch (command)
    {
        case 'e':
            // ROS asks for encoders. We return cumulative ticks separated
            // by a space. NO "OK" text is permitted here.
            // NOTE: does NOT reset the watchdog - see 'm'/'o' below.
            Serial.print(encoders.getLeftTicks());
            Serial.print(" ");
            Serial.println(encoders.getRightTicks());
            break;

        case 'm':
            // ROS sets target velocities. We pass them directly to the
            // PID controllers. This is a live motion command, so it
            // resets the watchdog and switches control to PID mode.
            watchdogReset();
            currentMode = MODE_PID;
            leftPID.setTarget(arg1);
            rightPID.setTarget(arg2);
            break;

        case 'r':
            // Resetting encoder counters is not a motion command -
            // does NOT reset the watchdog.
            encoders.reset();
            break;

        case 'o':
            // Raw open-loop PWM. Live motion command, so it resets the
            // watchdog and switches control to open-loop mode so the
            // PID block in loop() does not immediately overwrite this.
            watchdogReset();
            currentMode = MODE_OPEN_LOOP;
            motorSetPWM((int)arg1, (int)arg2);
            break;

        case 'p':
            leftPID.setTunings(arg1, arg2, arg3);
            rightPID.setTunings(arg1, arg2, arg3);
            break;

        case 's':
            currentMode = MODE_PID;
            motorSetPWM(0, 0);
            leftPID.reset();
            rightPID.reset();
            break;

        default:
            // Do nothing to avoid polluting the buffer if trash data is
            // received.
            break;
    }
}
