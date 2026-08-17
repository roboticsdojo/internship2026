/*
  Auracle Arduino Nano firmware
  -----------------------------
  Speaks the same serial protocol expected by joshnewans/diffdrive_arduino:

    e\r              -> "<left_ticks> <right_ticks>\r\n"
    m <l_cps> <r_cps>\r -> sets motor speed in encoder-counts-per-loop, no reply body
    u <p>:<d>:<i>:<o>\r -> sets PID gains (kept for compatibility, no-op if you
                           don't run closed-loop speed control here)
    r\r              -> reset encoder counts to 0

  NEW:
    i\r              -> "<gx> <gy> <gz> <ax> <ay> <az>\r\n"
                         gyro in rad/s, accel in m/s^2, MPU6050 raw I2C read
                         (no external library so it fits comfortably on a Nano)

  Wiring assumptions (change pin numbers to match your actual build):
    - 2x L298N, one per side. Each side's two DC motors are wired in parallel
      to the same H-bridge output, so ros2_control only ever sees one
      "left_wheel_joint" / "right_wheel_joint".
    - 4x A3144E hall-effect sensors as simple pulse encoders (no direction
      output), one per motor, ORed in pairs per side onto two interrupt pins.
      Direction is inferred from the commanded PWM sign, not from the sensor.
    - MPU6050 (GY-521 breakout) on I2C: SDA -> A4, SCL -> A5, addr 0x68.

  NOTE ON ORIENTATION: the MPU6050 has no magnetometer, so it cannot give you
  an absolute yaw. This firmware does NOT attempt sensor fusion on-board -
  it just streams raw angular velocity and linear acceleration and lets
  robot_localization's EKF (running on the Pi) do the fusion with wheel
  odometry. That's the standard, correct place to do it.
*/

#include <Wire.h>

// ---------------- Pin configuration (EDIT TO MATCH YOUR WIRING) ----------------
const uint8_t LEFT_ENC_PIN    = 2;   // interrupt-capable pin
const uint8_t RIGHT_ENC_PIN   = 3;   // interrupt-capable pin

const uint8_t LEFT_IN1        = 5;
const uint8_t LEFT_IN2        = 6;
const uint8_t LEFT_EN         = 9;   // PWM

const uint8_t RIGHT_IN1       = 7;
const uint8_t RIGHT_IN2       = 8;
const uint8_t RIGHT_EN        = 10;  // PWM

// ---------------- Encoder state ----------------
volatile long left_ticks = 0;
volatile long right_ticks = 0;
volatile int8_t left_dir = 1;
volatile int8_t right_dir = 1;

void leftEncoderISR()  { left_ticks  += left_dir; }
void rightEncoderISR() { right_ticks += right_dir; }

// ---------------- MPU6050 raw I2C driver (no external library) ----------------
const uint8_t MPU_ADDR = 0x68;

// LSB sensitivities for the default power-on ranges we configure below:
// gyro: +/-250 deg/s  -> 131.0 LSB/(deg/s)
// accel: +/-2 g        -> 16384.0 LSB/g
const float GYRO_LSB_PER_DEGS   = 131.0f;
const float ACCEL_LSB_PER_G     = 16384.0f;
const float DEG_TO_RAD          = 0.017453293f;
const float G_TO_MPS2           = 9.80665f;

void mpuWriteReg(uint8_t reg, uint8_t val)
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

bool mpuInit()
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x75); // WHO_AM_I
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((int)MPU_ADDR, 1, true);
  if (Wire.available() < 1) return false;
  uint8_t whoami = Wire.read();
  if (whoami != 0x68) return false;

  mpuWriteReg(0x6B, 0x00); // PWR_MGMT_1: wake up, use internal clock
  mpuWriteReg(0x1B, 0x00); // GYRO_CONFIG: +/-250 deg/s
  mpuWriteReg(0x1C, 0x00); // ACCEL_CONFIG: +/-2g
  mpuWriteReg(0x1A, 0x03); // CONFIG: DLPF ~44Hz, cuts vibration noise from the chassis
  return true;
}

// Reads accel(x,y,z), temp, gyro(x,y,z) in one burst starting at 0x3B
bool mpuRead(float &gx, float &gy, float &gz, float &ax, float &ay, float &az)
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((int)MPU_ADDR, 14, true);
  if (Wire.available() < 14) return false;

  int16_t raw_ax = (Wire.read() << 8) | Wire.read();
  int16_t raw_ay = (Wire.read() << 8) | Wire.read();
  int16_t raw_az = (Wire.read() << 8) | Wire.read();
  Wire.read(); Wire.read(); // discard temperature
  int16_t raw_gx = (Wire.read() << 8) | Wire.read();
  int16_t raw_gy = (Wire.read() << 8) | Wire.read();
  int16_t raw_gz = (Wire.read() << 8) | Wire.read();

  gx = (raw_gx / GYRO_LSB_PER_DEGS) * DEG_TO_RAD;
  gy = (raw_gy / GYRO_LSB_PER_DEGS) * DEG_TO_RAD;
  gz = (raw_gz / GYRO_LSB_PER_DEGS) * DEG_TO_RAD;

  ax = (raw_ax / ACCEL_LSB_PER_G) * G_TO_MPS2;
  ay = (raw_ay / ACCEL_LSB_PER_G) * G_TO_MPS2;
  az = (raw_az / ACCEL_LSB_PER_G) * G_TO_MPS2;

  return true;
}

// ---------------- Motor control ----------------
void setLeftMotor(int counts_per_loop)
{
  left_dir = (counts_per_loop >= 0) ? 1 : -1;
  int pwm = constrain(abs(counts_per_loop), 0, 255);
  digitalWrite(LEFT_IN1, counts_per_loop >= 0 ? HIGH : LOW);
  digitalWrite(LEFT_IN2, counts_per_loop >= 0 ? LOW : HIGH);
  analogWrite(LEFT_EN, pwm);
}

void setRightMotor(int counts_per_loop)
{
  right_dir = (counts_per_loop >= 0) ? 1 : -1;
  int pwm = constrain(abs(counts_per_loop), 0, 255);
  digitalWrite(RIGHT_IN1, counts_per_loop >= 0 ? HIGH : LOW);
  digitalWrite(RIGHT_IN2, counts_per_loop >= 0 ? LOW : HIGH);
  analogWrite(RIGHT_EN, pwm);
}

// ---------------- Serial command handling ----------------
String cmd_buffer = "";
bool imu_ok = false;

void handleCommand(String cmd)
{
  cmd.trim();
  if (cmd.length() == 0) return;

  char c = cmd.charAt(0);

  if (c == 'e')
  {
    noInterrupts();
    long l = left_ticks;
    long r = right_ticks;
    interrupts();
    Serial.print(l);
    Serial.print(" ");
    Serial.print(r);
    Serial.print("\r\n");
  }
  else if (c == 'r')
  {
    noInterrupts();
    left_ticks = 0;
    right_ticks = 0;
    interrupts();
    Serial.print("OK\r\n");
  }
  else if (c == 'm')
  {
    int l_val = 0, r_val = 0;
    sscanf(cmd.c_str(), "m %d %d", &l_val, &r_val);
    setLeftMotor(l_val);
    setRightMotor(r_val);
    Serial.print("OK\r\n");
  }
  else if (c == 'u')
  {
    // PID gains accepted for protocol compatibility; no-op here since this
    // firmware runs open-loop velocity (direct PWM), not onboard closed-loop.
    Serial.print("OK\r\n");
  }
  else if (c == 'i')
  {
    float gx, gy, gz, ax, ay, az;
    if (imu_ok && mpuRead(gx, gy, gz, ax, ay, az))
    {
      Serial.print(gx, 6); Serial.print(" ");
      Serial.print(gy, 6); Serial.print(" ");
      Serial.print(gz, 6); Serial.print(" ");
      Serial.print(ax, 6); Serial.print(" ");
      Serial.print(ay, 6); Serial.print(" ");
      Serial.print(az, 6);
      Serial.print("\r\n");
    }
    else
    {
      // Hardware interface will treat a non-parsing reply as a dropped
      // sample rather than crashing - still prints a well-formed 6-tuple.
      Serial.print("0 0 0 0 0 0\r\n");
    }
  }
}

void setup()
{
  Serial.begin(57600);
  Wire.begin();
  Wire.setClock(400000); // 400kHz I2C, MPU6050 supports fast mode

  pinMode(LEFT_ENC_PIN, INPUT_PULLUP);
  pinMode(RIGHT_ENC_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(LEFT_ENC_PIN), leftEncoderISR, RISING);
  attachInterrupt(digitalPinToInterrupt(RIGHT_ENC_PIN), rightEncoderISR, RISING);

  pinMode(LEFT_IN1, OUTPUT);
  pinMode(LEFT_IN2, OUTPUT);
  pinMode(LEFT_EN, OUTPUT);
  pinMode(RIGHT_IN1, OUTPUT);
  pinMode(RIGHT_IN2, OUTPUT);
  pinMode(RIGHT_EN, OUTPUT);

  imu_ok = mpuInit();
}

void loop()
{
  while (Serial.available())
  {
    char c = Serial.read();
    if (c == '\r')
    {
      handleCommand(cmd_buffer);
      cmd_buffer = "";
    }
    else
    {
      cmd_buffer += c;
    }
  }
}