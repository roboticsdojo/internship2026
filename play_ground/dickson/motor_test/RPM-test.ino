/*
  Motor + Encoder RPM Test
  ------------------------
  Drives the motor forward while calculating RPM.

  PWM Pin : D5
  IN1     : D8
  IN2     : D9

  Encoder A : D2
  Encoder B : D3

  Serial Monitor : 115200 baud
*/

// H-Bridge Motor Driver Pin Definitions
const byte ENA = 5;  // Speed control pin (PWM)
const byte IN1 = 8;  // Direction control pin 1
const byte IN2 = 9;  // Direction control pin 2

// Encoder Signal Pin Definitions
const byte ENCODER_A = 2; // Channel A (Interrupt pin)
const byte ENCODER_B = 3; // Channel B

// Resolution calibration: Number of encoder edge changes in one full wheel rotation
const float TICKS_PER_REV = 670.0;

// Tracks total pulses. 'volatile' ensures live RAM reads within the ISR
volatile long encoderTicks = 0;

// Variables for calculating timed interval calculations
unsigned long previousMillis = 0; // Stores the last time RPM was calculated
long previousTicks = 0;           // Stores the snapshot count from the last calculation

// Motor speed setting (0 = Stopped, 255 = Full Speed)
int motorPWM = 255;

// Interrupt Service Routine: Runs instantly on any pulse state change on Pin 2
void encoderISR()
{
  bool A = digitalRead(ENCODER_A);
  bool B = digitalRead(ENCODER_B);

  // Quadrature decoding: match means forward, mismatch means backward
  if (A == B)
    encoderTicks++;
  else
    encoderTicks--;
}

void setup()
{
  // Initialize high-speed serial communication
  Serial.begin(115200);

  // Configure motor control pins as outputs
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  // Configure encoder pins as inputs with internal pull-up resistors
  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);

  // Trigger the ISR whenever Channel A switches state (LOW->HIGH or HIGH->LOW)
  attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderISR, CHANGE);

  // Set H-Bridge direction inputs to drive the motor FORWARD
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  
  // Send the speed command to the motor driver
  analogWrite(ENA, motorPWM);

  // Print text header to the screen
  Serial.println("================================");
  Serial.println("Motor RPM Test");
  Serial.println("Motor running FORWARD");
  Serial.println("================================");
}

void loop()
{
  // Non-blocking timer: Executes the block exactly every 1000ms (1 second)
  if (millis() - previousMillis >= 1000)
  {
    previousMillis = millis(); // Save the current time stamp

    // Atomic read: Pause interrupts to copy the 4-byte tick count safely
    noInterrupts();
    long ticks = encoderTicks;
    interrupts();

    // Calculate how many new encoder ticks occurred during this 1-second window
    long deltaTicks = ticks - previousTicks;
    previousTicks = ticks; // Save current tick count for the next second's math

    // RPM Calculation:
    // 1. abs(deltaTicks) guarantees a positive value regardless of rotation direction.
    // 2. Multiplying by 60.0 scales ticks-per-second up to ticks-per-minute.
    // 3. Dividing by TICKS_PER_REV converts raw ticks into complete physical revolutions.
    float rpm = (abs(deltaTicks) * 60.0) / TICKS_PER_REV;

    // Output data to the Serial Monitor
    Serial.print("PWM: ");
    Serial.print(motorPWM);

    Serial.print("   Total Ticks: ");
    Serial.print(ticks);

    Serial.print("   RPM: ");
    Serial.println(rpm, 2); // Print RPM fixed to 2 decimal places
  }
}
