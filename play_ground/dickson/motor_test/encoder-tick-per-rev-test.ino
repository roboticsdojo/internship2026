/*
 Encoder Tick Counter
 Counts quadrature encoder ticks.

 Open Serial Monitor at 115200 baud.

 Commands:
 r = reset count
*/

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
/*
 Encoder Tick Counter
 Counts quadrature encoder ticks.

 Open Serial Monitor at 115200 baud.

 Commands:
 r = reset count
*/

// Pin definitions for the single encoder channels
const byte ENCODER_A = 2; // Channel A (Connected to an interrupt-capable pin)
const byte ENCODER_B = 3; // Channel B

// Variable to store the encoder tick count.
// 'volatile' prevents compiler optimization, forcing live reads from RAM during interrupts.
volatile long encoderTicks = 0;

// Interrupt Service Routine (ISR) triggered by changes on ENCODER_A
void encoderISR()
{
    // Read the current physical logic state (HIGH/LOW) of both pins
    bool A = digitalRead(ENCODER_A);
    bool B = digitalRead(ENCODER_B);

    // Decoding logic: If both signal states match, it's spinning forward.
    // If the signal states are different, it's spinning backward.
    if (A == B)
        encoderTicks++;
    else
        encoderTicks--;
}

void setup()
{
    // Set pins as inputs and enable internal pull-up resistors
    pinMode(ENCODER_A, INPUT_PULLUP);
    pinMode(ENCODER_B, INPUT_PULLUP);

    // Link pin A to the ISR. This fires on any voltage fluctuation (CHANGE).
    attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderISR, CHANGE);

    // Start serial communication at 115200 baud rate
    Serial.begin(115200);

    // Display a user menu on startup
    Serial.println();
    Serial.println("================================");
    Serial.println("Encoder Tick Counter");
    Serial.println("Turn wheel manually.");
    Serial.println("Send 'r' to reset.");
    Serial.println("================================");
}

void loop()
{
    // Remembers the last printed value across loop cycles. 
    // Initialized to an arbitrary number so the first real read (even if 0) prints out.
    static long previous = 999999;

    // Check if the user has typed anything into the Serial Monitor input bar
    if (Serial.available())
    {
        char c = Serial.read(); // Read the incoming character

        // If the user sends the character 'r', reset the tick counter
        if (c == 'r')
        {
            noInterrupts();   // Disable interrupts to safely write to the 4-byte long variable
            encoderTicks = 0; // Clear the count
            interrupts();     // Re-enable interrupts immediately

            Serial.println("Counter Reset");
        }
    }

    // Safely copy the volatile tick counter to a local variable
    noInterrupts();
    long ticks = encoderTicks;
    interrupts();

    // Change-detection logic: Only print if the wheel actually moved
    if (ticks != previous)
    {
        previous = ticks; // Synchronize previous value with current value

        // Print the updated position
        Serial.print("Ticks = ");
        Serial.println(ticks);
    }
}

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