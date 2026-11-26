#ifndef AURACLE_HARDWARE_ROBOT_SYSTEM_HPP
#define AURACLE_HARDWARE_ROBOT_SYSTEM_HPP

#include <array>
#include <chrono>
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
  std::string device_ = "/dev/arduino";
  std::array<Wheel, 4> wheels_;           // FL, FR, RL, RR
  std::array<double, 10> imu_states_{};   // orientation xyzw, gyro xyz, accel xyz
  std::string imu_name_ = "imu_sensor";

  bool have_prev_ = false;
  Telemetry prev_;
  std::chrono::steady_clock::time_point last_tel_time_ = std::chrono::steady_clock::now();
  bool tel_gap_logged_ = false;
};

}  // namespace auracle_hardware

#endif  // AURACLE_HARDWARE_ROBOT_SYSTEM_HPP