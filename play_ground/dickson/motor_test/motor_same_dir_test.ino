// --- Pin Assignments for L298N Motor Driver ---
// Left Motor Control Pins
const int ENA = 5;   // Speed control pin (PWM) for Left Motor
const int IN1 = 7;   // Direction pin 1 for Left Motor
const int IN2 = 8;   // Direction pin 2 for Left Motor

// Right Motor Control Pins
const int ENB = 10;  // Speed control pin (PWM) for Right Motor
const int IN3 = 11;  // Direction pin 1 for Right Motor
const int IN4 = 12;  // Direction pin 2 for Right Motor

void setup() {
  // Initialize all motor control pins as outputs
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
}

void loop() {
  // --- 1. ROTATE FORWARD ---
  // Set full speed
  analogWrite(ENA, 255);
  analogWrite(ENB, 255);
  
  // Set directions to forward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  
  delay(3000); // Run forward for 3 seconds


  // --- 2. STOP ---
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  
  delay(1000); // Stay stopped for 1 second


  // --- 3. ROTATE REVERSE ---
  // Restore full speed
  analogWrite(ENA, 255);
  analogWrite(ENB, 255);
  // Set directions to reverse
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  
  delay(3000); // Run reverse for 3 seconds


  // --- 4. STOP (Optional but recommended before reping) c  // Stops the robot briefly before it abruptly snaps back to moving forward

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  
  delay(1000); // Stay stopped for 1 second before the loop restarts
}
