#ifndef AURACLE_HARDWARE_ROBOT_SYSTEM_HPP
#define AURACLE_HARDWARE_ROBOT_SYSTEM_HPP

#include <memory>
#include <string>
#include <vector>

#include "hardware_interface/handle.hpp"
#include "hardware_interface/hardware_info.hpp"
#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/state.hpp"

#include "auracle_hardware/arduino_comms.hpp"
#include "auracle_hardware/wheel.hpp"

namespace auracle_hardware
{

class AuracleHardwareInterface : public hardware_interface::SystemInterface
{
public:
  RCLCPP_SHARED_PTR_DEFINITIONS(AuracleHardwareInterface)

  hardware_interface::CallbackReturn on_init(const hardware_interface::HardwareInfo & info) override;
  hardware_interface::CallbackReturn on_configure(const rclcpp_lifecycle::State & previous_state) override;
  hardware_interface::CallbackReturn on_activate(const rclcpp_lifecycle::State & previous_state) override;
  hardware_interface::CallbackReturn on_deactivate(const rclcpp_lifecycle::State & previous_state) override;
  hardware_interface::CallbackReturn on_cleanup(const rclcpp_lifecycle::State & previous_state) override;

  std::vector<hardware_interface::StateInterface> export_state_interfaces() override;
  std::vector<hardware_interface::CommandInterface> export_command_interfaces() override;

  hardware_interface::return_type read(const rclcpp::Time & time, const rclcpp::Duration & period) override;
  hardware_interface::return_type write(const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  ArduinoComms comms_;

  Wheel wheel_l_;
  Wheel wheel_r_;

  // IMU state, indexed to match the <sensor name="imu_sensor"> block in
  // ros2_control.xacro: [ori.x/y/z/w, ang_vel.x/y/z, lin_acc.x/y/z]
  std::array<double, 10> imu_states_{};
  std::string imu_sensor_name_ = "imu_sensor";

  std::string device_ = "/dev/ttyUSB0";
  int32_t baud_rate_ = 115200;
  int32_t timeout_ms_ = 1000;
  double enc_counts_per_rev_ = 3436.0;
  double loop_rate_ = 30.0;

  // Set true once a full telemetry line has been parsed at least once, so we
  // don't report bogus zero velocity forever if the Arduino is simply slow
  // to send its first line.
  bool comms_up_ = false;
};

}  // namespace auracle_hardware

#endif  // AURACLE_HARDWARE_ROBOT_SYSTEM_HPP
