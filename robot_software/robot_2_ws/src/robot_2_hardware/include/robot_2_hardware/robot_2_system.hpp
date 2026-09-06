#ifndef ROBOT_2_HARDWARE__ROBOT_2_SYSTEM_HPP_
#define ROBOT_2_HARDWARE__ROBOT_2_SYSTEM_HPP_

#include <string>
#include <vector>

#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "hardware_interface/handle.hpp"
#include "rclcpp/macros.hpp"
#include "rclcpp_lifecycle/state.hpp"

namespace robot_2_hardware
{

class Robot2System : public hardware_interface::SystemInterface
{
public:

  RCLCPP_SHARED_PTR_DEFINITIONS(Robot2System)

  // ==========================================================
  // ROS 2 CONTROL LIFECYCLE
  // ==========================================================

  hardware_interface::CallbackReturn on_init(
    const hardware_interface::HardwareComponentInterfaceParams & params) override;

  hardware_interface::CallbackReturn on_configure(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::CallbackReturn on_activate(
    const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::CallbackReturn on_deactivate(
    const rclcpp_lifecycle::State & previous_state) override;


  // ==========================================================
  // INTERFACES
  // ==========================================================

  std::vector<hardware_interface::StateInterface>
  export_state_interfaces() override;

  std::vector<hardware_interface::CommandInterface>
  export_command_interfaces() override;


  // ==========================================================
  // HARDWARE COMMUNICATION
  // ==========================================================

  hardware_interface::return_type read(
    const rclcpp::Time & time,
    const rclcpp::Duration & period) override;

  hardware_interface::return_type write(
    const rclcpp::Time & time,
    const rclcpp::Duration & period) override;


private:

  // ==========================================================
  // SERIAL
  // ==========================================================

  bool openSerial();

  void closeSerial();

  bool configureSerial();

  bool writeSerial(
    const std::string & command);

  bool readLine(
    std::string & line,
    int timeout_ms);


  // ==========================================================
  // ARDUINO PROTOCOL
  // ==========================================================

  // Combined encoder + IMU query ('b' command) - replaces the old
  // separate requestEncoders()/requestImu() pair. Those required two
  // sequential blocking serial round trips per read() cycle, each
  // gated by the Arduino's loop() only checking Serial.available()
  // once per pass (with imu.update()'s I2C transaction also running
  // every loop() pass in between) - a meaningful chunk of the 33ms
  // controller_manager cycle budget was going into that second
  // round trip alone. One combined query halves the per-cycle
  // serial latency.
  //
  // Returns false on timeout or a malformed response. Unlike the old
  // requestEncoders()/requestImu() split, encoder and IMU data now
  // succeed or fail together as a single unit - see read()'s comment
  // for how this is handled.
  bool requestCombined(
    long & left_ticks, long & right_ticks,
    double & ax, double & ay, double & az,
    double & gx, double & gy, double & gz,
    double & roll, double & pitch, double & yaw);

  bool sendMotorCommand(
    double left_velocity,
    double right_velocity);


  // ==========================================================
  // CONVERSIONS
  // ==========================================================

  double velocityToTicksPerPeriod(
    double velocity) const;

  double ticksToRadians(
    long ticks) const;

  double ticksToRadians(
    double ticks) const;

  double ticksPerSecondToRadiansPerSecond(
    long delta_ticks,
    double period_seconds) const;

  void eulerToQuaternion(
    double roll, double pitch, double yaw,
    double & qx, double & qy, double & qz, double & qw) const;


  // ==========================================================
  // PARAMETER HELPERS
  // ==========================================================

  double getNumericParameter(
    const std::string & name,
    double default_value) const;

  std::string getStringParameter(
    const std::string & name,
    const std::string & default_value) const;


  // ==========================================================
  // JOINT INFORMATION
  // ==========================================================

  std::vector<std::string> joint_names_;


  // ==========================================================
  // ROS 2 CONTROL STORAGE
  //
  // Order:
  //
  // 0 = front_left
  // 1 = front_right
  // 2 = back_left
  // 3 = back_right
  // ==========================================================

  std::vector<double> hw_commands_;

  std::vector<double> hw_positions_;

  std::vector<double> hw_velocities_;


  // ==========================================================
  // IMU SENSOR STORAGE
  // ==========================================================

  bool imu_enabled_{false};

  std::string imu_sensor_name_;

  std::vector<double> imu_state_;

  std::vector<std::string> imu_interface_names_;


  // ==========================================================
  // SERIAL CONFIGURATION
  // ==========================================================

  std::string device_;

  // NOTE: default here is only used if the "baud_rate" hardware
  // parameter is absent from ros2_control.xacro entirely. The
  // firmware's actual SERIAL_BAUD (config.h) MUST match whatever
  // value ros2_control.xacro passes - raised to 115200 alongside the
  // firmware to roughly halve per-message transmission time.
  int baud_rate_{115200};

  int timeout_ms_{200};

  int serial_fd_{-1};


  // ==========================================================
  // ROBOT PARAMETERS
  // ==========================================================

  double encoder_ticks_per_rev_{1980.0};

  double pid_period_seconds_{0.05};

  double wheel_radius_{0.0425};

  double wheel_separation_{0.30};


  // ==========================================================
  // DIRECTION CORRECTIONS
  // ==========================================================

  double left_encoder_sign_{1.0};

  double right_encoder_sign_{1.0};

  double left_command_sign_{1.0};

  double right_command_sign_{1.0};


  // ==========================================================
  // ENCODER STATE
  // ==========================================================

  long previous_left_ticks_{0};

  long previous_right_ticks_{0};

  bool first_read_{true};


  // ==========================================================
  // HARDWARE STATE
  // ==========================================================

  bool active_{false};
};

}  // namespace robot_2_hardware

#endif  // ROBOT_2_HARDWARE__ROBOT_2_SYSTEM_HPP_