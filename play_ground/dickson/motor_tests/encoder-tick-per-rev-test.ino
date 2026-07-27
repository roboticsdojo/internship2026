/*
 Encoder Tick Counter
 Counts quadrature encoder ticks.

 Open Serial Monitor at 115200 baud.

 Commands:
 r = reset count
*/

// 2 and 3 are the only interrupt pin on the arduino uno

const byte ENCODER_A = 2; 
const byte ENCODER_B = 3;

volatile long encoderTicks = 0;

void encoderISR()
{
    bool A = digitalRead(ENCODER_A);
    bool B = digitalRead(ENCODER_B);

    if (A == B)
        encoderTicks++;
    else
        encoderTicks--;
}

void setup()
{
    pinMode(ENCODER_A, INPUT_PULLUP);
    pinMode(ENCODER_B, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderISR, CHANGE);

    Serial.begin(115200);

    Serial.println();
    Serial.println("================================");
    Serial.println("Encoder Tick Counter");
    Serial.println("Turn wheel manually.");
    Serial.println("Send 'r' to reset.");
    Serial.println("================================");
}

void loop()
{
    static long previous = 999999;

    if (Serial.available())
    {
        char c = Serial.read();

        if (c == 'r')
        {
            noInterrupts();
            encoderTicks = 0;
            interrupts();

            Serial.println("Counter Reset");
        }
    }

    noInterrupts();
    long ticks = encoderTicks;
    interrupts();

    if (ticks != previous)
    {
        previous = ticks;

        Serial.print("Ticks = ");
        Serial.println(ticks);
    }
}