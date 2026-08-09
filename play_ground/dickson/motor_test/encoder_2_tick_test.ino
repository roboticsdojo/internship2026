// Variables to store the encoder tick counts. 
// 'volatile' tells the compiler that these values can change at any time due to interrupts.
volatile long leftTicks = 0; 
volatile long rightTicks = 0; 

// Pin definitions for the left and right encoder channels (A and B)
const int LEFT_A = 2;   // Left encoder Channel A (Interrupt pin)
const int LEFT_B = 4;   // Left encoder Channel B
const int RIGHT_A = 3;  // Right encoder Channel A (Interrupt pin)
const int RIGHT_B = 5;  // Right encoder Channel B

void setup() {
  // Initialize serial communication at a fast baud rate of 115200
  Serial.begin(115200); 
  
  // Configure encoder pins as inputs with internal pull-up resistors enabled
  pinMode(LEFT_A, INPUT_PULLUP);
  pinMode(LEFT_B, INPUT_PULLUP);
  pinMode(RIGHT_A, INPUT_PULLUP);
  pinMode(RIGHT_B, INPUT_PULLUP);
  
  // Attach Interrupt Service Routines (ISRs) to Channel A pins.
  // The ISR fires on any state CHANGE (LOW to HIGH or HIGH to LOW).
  attachInterrupt(digitalPinToInterrupt(LEFT_A), leftEncoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(RIGHT_A), rightEncoderISR, CHANGE);
}

void loop() {
  // Keeps track of the last time data was printed
  static unsigned long lastPrint = 0; 
  
  // Non-blocking timer: triggers every 100 milliseconds
  if (millis() - lastPrint >= 100) { 
    
    // Disable interrupts temporarily to safely read 4-byte 'long' variables.
    // This prevents the ISR from corrupting data mid-read on 8-bit microcontrollers.
    noInterrupts(); 
    long left = leftTicks; 
    long right = rightTicks; 
    interrupts(); // Re-enable interrupts immediately
    
    // Print the safely copied tick values to the Serial Monitor
    Serial.print("L:"); 
    Serial.print(left); 
    Serial.print(" R:"); 
    Serial.println(right); 
    
    // Update the timestamp for the next 100ms interval
    lastPrint = millis(); 
  }
}

// Interrupt Service Routine for the left encoder
void leftEncoderISR() {
  // Quadrature decoding logic: check if both channels match
  if (digitalRead(LEFT_A) == digitalRead(LEFT_B)) {
    leftTicks++; // Moving forward
  } else {
    leftTicks--; // Moving backward
  }
}

// Interrupt Service Routine for the right encoder
void rightEncoderISR() {
  // Quadrature decoding logic: check if both channels match
  if (digitalRead(RIGHT_A) == digitalRead(RIGHT_B)) {
    rightTicks++; // Moving forward
  } else {
    rightTicks--; // Moving backward
  }
}
