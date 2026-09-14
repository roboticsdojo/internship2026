#include "auracle_hardware/robot_system.hpp"

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "pluginlib/class_list_macros.hpp"

namespace auracle_hardware
{

namespace
{
// Little helper so a missing/blank param doesn't throw and take down the
// controller_manager - it falls back to the default and logs instead.
std::string getParam(
  const std::unordered_map<std::string, std::string> & params,
  const std::string & key, const std::string & default_value)
{
  auto it = params.find(key);
  if (it == params.end() || it->second.empty()) {
    return default_value;
  }
  return it->second;
}
}  // namespace

hardware_interface::CallbackReturn AuracleHardwareInterface::on_init(
  const hardware_interface::HardwareInfo & info)
{
  if (hardware_interface::SystemInterface::on_init(info) != hardware_interface::CallbackReturn::SUCCESS) {
    return hardware_interface::CallbackReturn::ERROR;
  }

  device_ = getParam(info_.hardware_parameters, "device", device_);
  baud_rate_ = std::stoi(getParam(info_.hardware_parameters, "baud_rate", "115200"));
  timeout_ms_ = std::stoi(getParam(info_.hardware_parameters, "timeout_ms", "1000"));
  enc_counts_per_rev_ = std::stod(getParam(info_.hardware_parameters, "enc_counts_per_rev", "3436"));
  loop_rate_ = std::stod(getParam(info_.hardware_parameters, "loop_rate", "30"));
  imu_sensor_name_ = getParam(info_.hardware_parameters, "imu_sensor_name", "imu_sensor");

  std::string left_wheel_name = getParam(info_.hardware_parameters, "left_wheel_name", "left_wheel_two_joint");
  std::string right_wheel_name = getParam(info_.hardware_parameters, "right_wheel_name", "right_wheel_two_joint");
  wheel_l_.setup(left_wheel_name, enc_counts_per_rev_);
  wheel_r_.setup(right_wheel_name, enc_counts_per_rev_);

  // Sanity-check that the joints ros2_control gave us actually match the
  // wheel names we were just configured with, so a typo in the xacro fails
  // loudly at startup instead of silently controlling nothing.
  for (const auto & joint : info_.joints) {
    if (joint.name != wheel_l_.name && joint.name != wheel_r_.name) {
      RCLCPP_WARN(
        rclcpp::get_logger("AuracleHardwareInterface"),
        "Joint '%s' declared in ros2_control.xacro is not one of the configured wheel names "
        "('%s', '%s') - it will not be driven.",
        joint.name.c_str(), wheel_l_.name.c_str(), wheel_r_.name.c_str());
    }
  }

  RCLCPP_INFO(
    rclcpp::get_logger("AuracleHardwareInterface"),
    "Configured for device=%s baud=%d enc_counts_per_rev=%.1f wheels=[%s, %s]",
    device_.c_str(), baud_rate_, enc_counts_per_rev_, wheel_l_.name.c_str(), wheel_r_.name.c_str());

  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn AuracleHardwareInterface::on_configure(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  RCLCPP_INFO(rclcpp::get_logger("AuracleHardwareInterface"), "Opening serial port %s ...", device_.c_str());
  try {
    comms_.connect(device_, baud_rate_, timeout_ms_);
  } catch (const std::exception & e) {
    RCLCPP_FATAL(
      rclcpp::get_logger("AuracleHardwareInterface"), "Failed to open serial port %s: %s",
      device_.c_str(), e.what());
    return hardware_interface::CallbackReturn::ERROR;
  }

  // Firmware sends "READY\n" once out of setup(). Give it a couple of
  // seconds (Nano resets on port-open) - if it never shows up we still
  // proceed, since older firmware builds may not send it, but we log loudly.
  if (!comms_.waitForReady(3000)) {
    RCLCPP_WARN(
      rclcpp::get_logger("AuracleHardwareInterface"),
      "Did not see a READY handshake from the Arduino within 3s - continuing anyway, "
      "but check wiring/firmware if reads keep failing.");
  }

  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn AuracleHardwareInterface::on_cleanup(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  if (comms_.connected()) {
    comms_.disconnect();
  }
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn AuracleHardwareInterface::on_activate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  wheel_l_.command = 0;
  wheel_r_.command = 0;
  comms_.sendVelocity(0.0, 0.0);
  RCLCPP_INFO(rclcpp::get_logger("AuracleHardwareInterface"), "Activated.");
  return hardware_interface::CallbackReturn::SUCCESS;
}

hardware_interface::CallbackReturn AuracleHardwareInterface::on_deactivate(
  const rclcpp_lifecycle::State & /*previous_state*/)
{
  // Always leave the robot stopped when the controller deactivates
  // (e.g. controller_manager shutdown, ctrl-C) rather than coasting at
  // its last commanded velocity.
  comms_.sendVelocity(0.0, 0.0);
  RCLCPP_INFO(rclcpp::get_logger("AuracleHardwareInterface"), "Deactivated - motors stopped.");
  return hardware_interface::CallbackReturn::SUCCESS;
}

std::vector<hardware_interface::StateInterface> AuracleHardwareInterface::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> state_interfaces;

  state_interfaces.emplace_back(wheel_l_.name, hardware_interface::HW_IF_POSITION, &wheel_l_.position);
  state_interfaces.emplace_back(wheel_l_.name, hardware_interface::HW_IF_VELOCITY, &wheel_l_.velocity);
  state_interfaces.emplace_back(wheel_r_.name, hardware_interface::HW_IF_POSITION, &wheel_r_.position);
  state_interfaces.emplace_back(wheel_r_.name, hardware_interface::HW_IF_VELOCITY, &wheel_r_.velocity);

  static const char * imu_interfaces[10] = {
    "orientation.x", "orientation.y", "orientation.z", "orientation.w",
    "angular_velocity.x", "angular_velocity.y", "angular_velocity.z",
    "linear_acceleration.x", "linear_acceleration.y", "linear_acceleration.z"};
  for (size_t i = 0; i < imu_states_.size(); ++i) {
    state_interfaces.emplace_back(imu_sensor_name_, imu_interfaces[i], &imu_states_[i]);
  }

  return state_interfaces;
}

std::vector<hardware_interface::CommandInterface> AuracleHardwareInterface::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> command_interfaces;
  command_interfaces.emplace_back(wheel_l_.name, hardware_interface::HW_IF_VELOCITY, &wheel_l_.command);
  command_interfaces.emplace_back(wheel_r_.name, hardware_interface::HW_IF_VELOCITY, &wheel_r_.command);
  return command_interfaces;
}

hardware_interface::return_type AuracleHardwareInterface::read(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & period)
{
  long l_enc = wheel_l_.enc_ticks;
  long r_enc = wheel_r_.enc_ticks;
  float acc[3] = {0, 0, 0};
  float gyro[3] = {0, 0, 0};

  if (!comms_.readTelemetry(l_enc, r_enc, acc, gyro)) {
    // No fresh line this cycle (typical at 30Hz loop vs whatever the read()
    // period actually is) - just keep the last known state rather than
    // erroring the whole hardware component out.
    return hardware_interface::return_type::OK;
  }
  comms_up_ = true;

  double new_position_l = wheel_l_.ticksToRadians(l_enc);
  double new_position_r = wheel_r_.ticksToRadians(r_enc);

  double dt = period.seconds();
  if (dt > 0.0) {
    wheel_l_.velocity = (new_position_l - wheel_l_.position) / dt;
    wheel_r_.velocity = (new_position_r - wheel_r_.position) / dt;
  }
  wheel_l_.position = new_position_l;
  wheel_r_.position = new_position_r;
  wheel_l_.enc_ticks = l_enc;
  wheel_r_.enc_ticks = r_enc;

  // MPU6050 has no magnetometer -> no absolute orientation. Report an
  // identity quaternion (matches static_covariance_orientation: [-1, ...]
  // in my_controllers.yaml, which tells imu_sensor_broadcaster/EKF this
  // field is not to be trusted) and pass gyro/accel straight through.
  imu_states_[0] = 0.0; imu_states_[1] = 0.0; imu_states_[2] = 0.0; imu_states_[3] = 1.0;
  imu_states_[4] = gyro[0]; imu_states_[5] = gyro[1]; imu_states_[6] = gyro[2];
  imu_states_[7] = acc[0]; imu_states_[8] = acc[1]; imu_states_[9] = acc[2];

  return hardware_interface::return_type::OK;
}

hardware_interface::return_type AuracleHardwareInterface::write(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  comms_.sendVelocity(wheel_l_.command, wheel_r_.command);
  return hardware_interface::return_type::OK;
}

}  // namespace auracle_hardware

PLUGINLIB_EXPORT_CLASS(auracle_hardware::AuracleHardwareInterface, hardware_interface::SystemInterface)
