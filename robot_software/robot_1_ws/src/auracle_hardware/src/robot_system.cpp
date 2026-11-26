#include "auracle_hardware/robot_system.hpp"

#include <chrono>
#include <cmath>

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "pluginlib/class_list_macros.hpp"

namespace auracle_hardware
{

namespace
{
constexpr size_t FL = 0;
constexpr size_t FR = 1;
constexpr size_t RL = 2;
constexpr size_t RR = 3;

// MPU6050 at +/-2 g, +/-250 deg/s (set in firmware).
constexpr double ACCEL_SCALE = 9.80665 / 16384.0;
constexpr double GYRO_SCALE = (M_PI / 180.0) / 131.0;

constexpr int HANDSHAKE_TIMEOUT_MS = 5000;

const char * faultName(int code)
{
  switch (code) {
    case 1: return "WRONG_SIGN (wheel moving against the command: encoder sign/wiring)";
    case 2: return "SATURATED (PWM at 255, target not reached: too fast for load/battery)";
    case 3: return "NO_MOTION (PWM applied but wheel barely moves: stalled wheel or dead encoder)";
    default: return "UNKNOWN";
  }
}

rclcpp::Logger logger() {return rclcpp::get_logger("AuracleHardwareInterface");}

std::string param(const hardware_interface::HardwareInfo & info, const std::string & key,
  const std::string & fallback)
{
  auto it = info.hardware_parameters.find(key);
  return it == info.hardware_parameters.end() ? fallback : it->second;
}
}  // namespace

hardware_interface::CallbackReturn AuracleHardwareInterface::on_init(
  const hardware_interface::HardwareInfo & info)
{
  if (hardware_interface::SystemInterface::on_init(info) !=
    hardware_interface::CallbackReturn::SUCCESS)
  {
    return hardware_interface::CallbackReturn::ERROR;
  }

  device_ = param(info_, "device", device_);
  imu_name_ = param(info_, "imu_sensor_name", imu_name_);
  double left_cpr = std::stod(param(info_, "left_enc_counts_per_rev", "985.0"));
  double right_cpr = std::stod(param(info_, "right_enc_counts_per_rev", "990.0"));
  double left_sign = std::stod(param(info_, "left_direction_sign", "1.0"));
  double right_sign = std::stod(param(info_, "right_direction_sign", "1.0"));

  wheels_[FL].setup(param(info_, "front_left_wheel_name", "left_wheel_one_joint"), left_cpr, left_sign);
  wheels_[FR].setup(param(info_, "front_right_wheel_name", "right_wheel_one_joint"), right_cpr, right_sign);
  wheels_[RL].setup(param(info_, "rear_left_wheel_name", "left_wheel_three_joint"), left_cpr, left_sign);
  wheels_[RR].setup(param(info_, "rear_right_wheel_name", "right_wheel_three_joint"), right_cpr, right_sign);

  imu_states_[3] = 1.0;  // identity orientation - the MPU6050 gives none
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn AuracleHardwareInterface::on_configure(
  const rclcpp_lifecycle::State &)
{
  try {
    comms_.connect(device_);
  } catch (const std::exception & e) {
    RCLCPP_FATAL(logger(), "Can't open %s: %s", device_.c_str(), e.what());
    return hardware_interface::CallbackReturn::ERROR;
  }
  // Nano resets on open: bootloader + ~0.5 s gyro calibration, keep still.
  if (!comms_.waitForReady(HANDSHAKE_TIMEOUT_MS)) {
    RCLCPP_WARN(logger(), "No READY from %s - continuing anyway.", device_.c_str());
  }
  have_prev_ = false;
  last_tel_time_ = std::chrono::steady_clock::now();
  tel_gap_logged_ = false;
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn AuracleHardwareInterface::on_cleanup(
  const rclcpp_lifecycle::State &)
{
  if (comms_.connected()) {
    comms_.sendStop();
    comms_.disconnect();
  }
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn AuracleHardwareInterface::on_activate(
  const rclcpp_lifecycle::State &)
{
  for (auto & w : wheels_) {
    w.command = 0.0;
  }
  comms_.sendVelocity(0.0, 0.0);
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn AuracleHardwareInterface::on_deactivate(
  const rclcpp_lifecycle::State &)
{
  comms_.sendStop();
  return hardware_interface::CallbackReturn::SUCCESS;
}

std::vector<hardware_interface::StateInterface>
AuracleHardwareInterface::export_state_interfaces()
{
  static const char * imu_ifaces[10] = {
    "orientation.x", "orientation.y", "orientation.z", "orientation.w",
    "angular_velocity.x", "angular_velocity.y", "angular_velocity.z",
    "linear_acceleration.x", "linear_acceleration.y", "linear_acceleration.z"};

  std::vector<hardware_interface::StateInterface> s;
  for (auto & w : wheels_) {
    s.emplace_back(w.name, hardware_interface::HW_IF_POSITION, &w.position);
    s.emplace_back(w.name, hardware_interface::HW_IF_VELOCITY, &w.velocity);
  }
  for (size_t i = 0; i < imu_states_.size(); ++i) {
    s.emplace_back(imu_name_, imu_ifaces[i], &imu_states_[i]);
  }
  return s;
}

std::vector<hardware_interface::CommandInterface>
AuracleHardwareInterface::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> c;
  for (auto & w : wheels_) {
    c.emplace_back(w.name, hardware_interface::HW_IF_VELOCITY, &w.command);
  }
  return c;
}

hardware_interface::return_type AuracleHardwareInterface::read(
  const rclcpp::Time &, const rclcpp::Duration &)
{
  Telemetry tel;
  const bool got_tel = comms_.readLatest(tel);

  // Nano wheel faults: it stops all motors and ignores motion commands until it retries.
  for (const auto & f : comms_.takeFaults()) {
    if (f.has_detail) {
      RCLCPP_ERROR(
        logger(), "Nano FAULT on %c wheel: code %d %s | speed %d ticks/s, target %d ticks/s",
        f.side, f.code, faultName(f.code), f.speed, f.target);
    } else {
      RCLCPP_ERROR(logger(), "Nano FAULT on %c wheel: code %d %s", f.side, f.code, faultName(f.code));
    }
  }

  // Telemetry is 20 Hz; a gap means the serial link or the Nano is in trouble.
  const auto now = std::chrono::steady_clock::now();
  if (got_tel) {
    last_tel_time_ = now;
    tel_gap_logged_ = false;
  } else if (!tel_gap_logged_ && now - last_tel_time_ > std::chrono::seconds(1)) {
    RCLCPP_WARN(logger(), "No telemetry from the Nano for >1 s (serial link down or Nano stuck).");
    tel_gap_logged_ = true;
  }
  if (!got_tel) {
    return hardware_interface::return_type::OK;
  }

  // Integrate tick deltas using the Nano's own clock. If its clock went
  // backwards the Nano reset (counters back to 0) - just re-sync.
  if (have_prev_ && tel.t_ms > prev_.t_ms) {
    double dt = (tel.t_ms - prev_.t_ms) / 1000.0;
    double dl = wheels_[RL].ticksToRadians(static_cast<double>(tel.l_enc - prev_.l_enc));
    double dr = wheels_[RR].ticksToRadians(static_cast<double>(tel.r_enc - prev_.r_enc));
    wheels_[RL].position += dl;
    wheels_[RR].position += dr;
    wheels_[RL].velocity = dl / dt;
    wheels_[RR].velocity = dr / dt;
  } else {
    if (have_prev_ && tel.t_ms < prev_.t_ms) {
      RCLCPP_WARN(
        logger(), "Nano clock went backwards (%u -> %u ms): it RESET (brownout, hardware watchdog "
        "or serial reopen). Wheel odometry re-synced.", prev_.t_ms, tel.t_ms);
    }
    wheels_[RL].velocity = 0.0;
    wheels_[RR].velocity = 0.0;
  }
  prev_ = tel;
  have_prev_ = true;

  // Front wheels have no encoders: mirror the same-side rear wheel.
  wheels_[FL].position = wheels_[RL].position;
  wheels_[FL].velocity = wheels_[RL].velocity;
  wheels_[FR].position = wheels_[RR].position;
  wheels_[FR].velocity = wheels_[RR].velocity;

  for (size_t i = 0; i < 3; ++i) {
    imu_states_[4 + i] = tel.imu[3 + i] * GYRO_SCALE;
    imu_states_[7 + i] = tel.imu[i] * ACCEL_SCALE;
  }
  return hardware_interface::return_type::OK;
}

hardware_interface::return_type AuracleHardwareInterface::write(
  const rclcpp::Time &, const rclcpp::Duration &)
{
  // One closed loop per side on the Nano; average the two commands.
  // Sent every cycle - it's also the heartbeat for the 500 ms watchdog.
  double left = (wheels_[FL].command + wheels_[RL].command) / 2.0;
  double right = (wheels_[FR].command + wheels_[RR].command) / 2.0;
  comms_.sendVelocity(wheels_[RL].radiansToTicks(left), wheels_[RR].radiansToTicks(right));
  return hardware_interface::return_type::OK;
}

}  // namespace auracle_hardware

PLUGINLIB_EXPORT_CLASS(auracle_hardware::AuracleHardwareInterface, hardware_interface::SystemInterface)