#include "robot_2_hardware/robot_2_system.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sstream>
#include <termios.h>
#include <unistd.h>

#include "pluginlib/class_list_macros.hpp"

namespace robot_2_hardware
{

// ============================================================
// Constructor / Initialization
// ============================================================

hardware_interface::CallbackReturn Robot2System::on_init(
  const hardware_interface::HardwareInfo & info)
{
  if (
    hardware_interface::SystemInterface::on_init(info) !=
    hardware_interface::CallbackReturn::SUCCESS)
  {
    return hardware_interface::CallbackReturn::ERROR;
  }

  // ----------------------------------------------------------
  // Validate number of joints
  // ----------------------------------------------------------

  if (info_.joints.size() != 4) {
    RCLCPP_ERROR(
      rclcpp::get_logger("robot_2_hardware"),
      "Expected exactly 4 wheel joints, got %zu",
      info_.joints.size());

    return hardware_interface::CallbackReturn::ERROR;
  }

  joint_names_.clear();

  for (const auto & joint : info_.joints) {
    joint_names_.push_back(joint.name);

    // --------------------------------------------------------
    // Each joint must have one velocity command interface
    // --------------------------------------------------------

    if (joint.command_interfaces.size() != 1 ||
        joint.command_interfaces[0].name !=
        hardware_interface::HW_IF_VELOCITY)
    {
      RCLCPP_ERROR(
        rclcpp::get_logger("robot_2_hardware"),
        "Joint '%s' must have exactly one velocity command interface",
        joint.name.c_str());

      return hardware_interface::CallbackReturn::ERROR;
    }

    // --------------------------------------------------------
    // Position + velocity state interfaces
    // --------------------------------------------------------

    if (joint.state_interfaces.size() != 2) {
      RCLCPP_ERROR(
        rclcpp::get_logger("robot_2_hardware"),
        "Joint '%s' must have position and velocity state interfaces",
        joint.name.c_str());

      return hardware_interface::CallbackReturn::ERROR;
    }

    bool has_position = false;
    bool has_velocity = false;

    for (const auto & state_interface : joint.state_interfaces) {
      if (state_interface.name == hardware_interface::HW_IF_POSITION) {
        has_position = true;
      }

      if (state_interface.name == hardware_interface::HW_IF_VELOCITY) {
        has_velocity = true;
      }
    }

    if (!has_position || !has_velocity) {
      RCLCPP_ERROR(
        rclcpp::get_logger("robot_2_hardware"),
        "Joint '%s' must provide position and velocity state interfaces",
        joint.name.c_str());

      return hardware_interface::CallbackReturn::ERROR;
    }
  }

  // ----------------------------------------------------------
  // Allocate storage
  // ----------------------------------------------------------

  hw_commands_.assign(info_.joints.size(), 0.0);
  hw_positions_.assign(info_.joints.size(), 0.0);
  hw_velocities_.assign(info_.joints.size(), 0.0);

  // ----------------------------------------------------------
  // Hardware parameters
  // ----------------------------------------------------------

  device_ = getStringParameter(
    "device",
    "/dev/ttyACM0");

  baud_rate_ = static_cast<int>(
    getNumericParameter(
      "baud_rate",
      57600));

  timeout_ms_ = static_cast<int>(
    getNumericParameter(
      "timeout_ms",
      200));

  encoder_ticks_per_rev_ =
    getNumericParameter(
      "encoder_ticks_per_rev",
      1980.0);

  pid_period_seconds_ =
    getNumericParameter(
      "pid_period",
      0.05);

  wheel_radius_ =
    getNumericParameter(
      "wheel_radius",
      0.0425);

  wheel_separation_ =
    getNumericParameter(
      "wheel_separation",
      0.30);

  left_encoder_sign_ =
    getNumericParameter(
      "left_encoder_sign",
      1.0);

  right_encoder_sign_ =
    getNumericParameter(
      "right_encoder_sign",
      1.0);

  left_command_sign_ =
    getNumericParameter(
      "left_command_sign",
      1.0);

  right_command_sign_ =
    getNumericParameter(
      "right_command_sign",
      1.0);

  // ----------------------------------------------------------
  // Initial state
  // ----------------------------------------------------------

  previous_left_ticks_ = 0;
  previous_right_ticks_ = 0;

  first_read_ = true;
  active_ = false;

  RCLCPP_INFO(
    rclcpp::get_logger("robot_2_hardware"),
    "Robot 2 hardware interface initialized");

  RCLCPP_INFO(
    rclcpp::get_logger("robot_2_hardware"),
    "Serial device: %s",
    device_.c_str());

  RCLCPP_INFO(
    rclcpp::get_logger("robot_2_hardware"),
    "Baud rate: %d",
    baud_rate_);

  RCLCPP_INFO(
    rclcpp::get_logger("robot_2_hardware"),
    "Encoder ticks/rev: %.2f",
    encoder_ticks_per_rev_);

  RCLCPP_INFO(
    rclcpp::get_logger("robot_2_hardware"),
    "Wheel radius: %.4f m",
    wheel_radius_);

  return hardware_interface::CallbackReturn::SUCCESS;
}


// ============================================================
// Configure
// ============================================================

hardware_interface::CallbackReturn Robot2System::on_configure(
  const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(
    rclcpp::get_logger("robot_2_hardware"),
    "Configuring Robot 2 hardware");

  if (!openSerial()) {
    RCLCPP_ERROR(
      rclcpp::get_logger("robot_2_hardware"),
      "Failed to open serial device %s",
      device_.c_str());

    return hardware_interface::CallbackReturn::ERROR;
  }

  // Reset Arduino encoder counters
  if (!writeSerial("r\n")) {
    RCLCPP_WARN(
      rclcpp::get_logger("robot_2_hardware"),
      "Could not send encoder reset command");
  }

  previous_left_ticks_ = 0;
  previous_right_ticks_ = 0;
  first_read_ = true;

  return hardware_interface::CallbackReturn::SUCCESS;
}


// ============================================================
// Activate
// ============================================================

hardware_interface::CallbackReturn Robot2System::on_activate(
  const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(
    rclcpp::get_logger("robot_2_hardware"),
    "Activating Robot 2 hardware");

  // Ensure motors are stopped before operation
  if (!writeSerial("s\n")) {
    RCLCPP_WARN(
      rclcpp::get_logger("robot_2_hardware"),
      "Failed to send stop command during activation");
  }

  std::fill(
    hw_commands_.begin(),
    hw_commands_.end(),
    0.0);

  std::fill(
    hw_positions_.begin(),
    hw_positions_.end(),
    0.0);

  std::fill(
    hw_velocities_.begin(),
    hw_velocities_.end(),
    0.0);

  previous_left_ticks_ = 0;
  previous_right_ticks_ = 0;
  first_read_ = true;

  active_ = true;

  return hardware_interface::CallbackReturn::SUCCESS;
}


// ============================================================
// Deactivate
// ============================================================

hardware_interface::CallbackReturn Robot2System::on_deactivate(
  const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(
    rclcpp::get_logger("robot_2_hardware"),
    "Deactivating Robot 2 hardware");

  writeSerial("s\n");

  active_ = false;

  return hardware_interface::CallbackReturn::SUCCESS;
}


// ============================================================
// Export state interfaces
// ============================================================

std::vector<hardware_interface::StateInterface>
Robot2System::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface>
    state_interfaces;

  for (std::size_t i = 0; i < joint_names_.size(); ++i) {

    state_interfaces.emplace_back(
      joint_names_[i],
      hardware_interface::HW_IF_POSITION,
      &hw_positions_[i]);

    state_interfaces.emplace_back(
      joint_names_[i],
      hardware_interface::HW_IF_VELOCITY,
      &hw_velocities_[i]);
  }

  return state_interfaces;
}


// ============================================================
// Export command interfaces
// ============================================================

std::vector<hardware_interface::CommandInterface>
Robot2System::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface>
    command_interfaces;

  for (std::size_t i = 0; i < joint_names_.size(); ++i) {

    command_interfaces.emplace_back(
      joint_names_[i],
      hardware_interface::HW_IF_VELOCITY,
      &hw_commands_[i]);
  }

  return command_interfaces;
}


// ============================================================
// READ
//
// Arduino command:
//
// e
//
// Arduino response:
//
// left_ticks right_ticks
//
// The Arduino returns cumulative encoder counts.
// We calculate the delta between reads and convert it
// into wheel position and velocity.
// ============================================================

hardware_interface::return_type Robot2System::read(
  const rclcpp::Time &,
  const rclcpp::Duration & period)
{
  if (!active_) {
    return hardware_interface::return_type::ERROR;
  }

  long left_ticks = 0;
  long right_ticks = 0;

  if (!requestEncoders(left_ticks, right_ticks)) {

    RCLCPP_ERROR(
      rclcpp::get_logger("robot_2_hardware"),
      "Failed to read encoder values");

    return hardware_interface::return_type::ERROR;
  }

  // ----------------------------------------------------------
  // Convert cumulative ticks to wheel positions
  // ----------------------------------------------------------

  const double left_position =
    ticksToRadians(
      static_cast<double>(left_ticks) *
      left_encoder_sign_);

  const double right_position =
    ticksToRadians(
      static_cast<double>(right_ticks) *
      right_encoder_sign_);

  // ----------------------------------------------------------
  // Calculate velocity
  // ----------------------------------------------------------

  const double period_seconds =
    period.seconds();

  if (first_read_) {

    hw_velocities_[0] = 0.0;
    hw_velocities_[1] = 0.0;
    hw_velocities_[2] = 0.0;
    hw_velocities_[3] = 0.0;

    first_read_ = false;

  } else if (period_seconds > 0.0) {

    const long delta_left =
      left_ticks - previous_left_ticks_;

    const long delta_right =
      right_ticks - previous_right_ticks_;

    const double left_velocity =
      ticksPerSecondToRadiansPerSecond(
        static_cast<long>(
          delta_left * left_encoder_sign_),
        period_seconds);

    const double right_velocity =
      ticksPerSecondToRadiansPerSecond(
        static_cast<long>(
          delta_right * right_encoder_sign_),
        period_seconds);

    // Both front and rear joints represent the same side
    hw_velocities_[0] = left_velocity;
    hw_velocities_[2] = left_velocity;

    hw_velocities_[1] = right_velocity;
    hw_velocities_[3] = right_velocity;
  }

  // ----------------------------------------------------------
  // Both joints on each side share the same encoder
  // ----------------------------------------------------------

  hw_positions_[0] = left_position;
  hw_positions_[2] = left_position;

  hw_positions_[1] = right_position;
  hw_positions_[3] = right_position;

  // ----------------------------------------------------------
  // Save encoder counts
  // ----------------------------------------------------------

  previous_left_ticks_ = left_ticks;
  previous_right_ticks_ = right_ticks;

  return hardware_interface::return_type::OK;
}


// ============================================================
// WRITE
//
// ros2_control gives wheel angular velocities in rad/s.
//
// Arduino firmware expects target encoder ticks per PID
// period.
//
// Therefore:
//
// rad/s
//   ↓
// rad / PID period
//   ↓
// revolutions / PID period
//   ↓
// encoder ticks / PID period
//
// The Arduino PID controller then controls PWM.
// ============================================================

hardware_interface::return_type Robot2System::write(
  const rclcpp::Time &,
  const rclcpp::Duration &)
{
  if (!active_) {
    return hardware_interface::return_type::ERROR;
  }

  // ----------------------------------------------------------
  // Average front/rear commands for each side
  // ----------------------------------------------------------

  const double left_velocity =
    (
      hw_commands_[0] +
      hw_commands_[2]
    ) / 2.0;

  const double right_velocity =
    (
      hw_commands_[1] +
      hw_commands_[3]
    ) / 2.0;

  // ----------------------------------------------------------
  // Apply command direction corrections
  // ----------------------------------------------------------

  const double corrected_left =
    left_velocity * left_command_sign_;

  const double corrected_right =
    right_velocity * right_command_sign_;

  // ----------------------------------------------------------
  // Convert rad/s to encoder ticks per PID period
  // ----------------------------------------------------------

  const double left_ticks =
    velocityToTicksPerPeriod(
      corrected_left);

  const double right_ticks =
    velocityToTicksPerPeriod(
      corrected_right);

  // ----------------------------------------------------------
  // Send command
  // ----------------------------------------------------------

  if (!sendMotorCommand(
      left_ticks,
      right_ticks))
  {
    RCLCPP_ERROR(
      rclcpp::get_logger("robot_2_hardware"),
      "Failed to send motor command");

    return hardware_interface::return_type::ERROR;
  }

  return hardware_interface::return_type::OK;
}


// ============================================================
// SERIAL OPEN
// ============================================================

bool Robot2System::openSerial()
{
  if (serial_fd_ >= 0) {
    return true;
  }

  serial_fd_ = ::open(
    device_.c_str(),
    O_RDWR | O_NOCTTY | O_NONBLOCK);

  if (serial_fd_ < 0) {

    RCLCPP_ERROR(
      rclcpp::get_logger("robot_2_hardware"),
      "Unable to open %s: %s",
      device_.c_str(),
      std::strerror(errno));

    return false;
  }

  if (!configureSerial()) {

    closeSerial();

    return false;
  }

  // Give Arduino time to reset after opening serial
  usleep(2000000);

  // Flush stale data
  tcflush(
    serial_fd_,
    TCIOFLUSH);

  RCLCPP_INFO(
    rclcpp::get_logger("robot_2_hardware"),
    "Serial connection established");

  return true;
}


// ============================================================
// SERIAL CLOSE
// ============================================================

void Robot2System::closeSerial()
{
  if (serial_fd_ >= 0) {

    ::close(serial_fd_);

    serial_fd_ = -1;
  }
}


// ============================================================
// SERIAL CONFIGURATION
// ============================================================

bool Robot2System::configureSerial()
{
  struct termios tty {};

  if (tcgetattr(serial_fd_, &tty) != 0) {

    RCLCPP_ERROR(
      rclcpp::get_logger("robot_2_hardware"),
      "tcgetattr failed: %s",
      std::strerror(errno));

    return false;
  }

  cfmakeraw(&tty);

  speed_t speed;

  switch (baud_rate_) {

    case 9600:
      speed = B9600;
      break;

    case 19200:
      speed = B19200;
      break;

    case 38400:
      speed = B38400;
      break;

    case 57600:
      speed = B57600;
      break;

    case 115200:
      speed = B115200;
      break;

    default:

      RCLCPP_ERROR(
        rclcpp::get_logger("robot_2_hardware"),
        "Unsupported baud rate: %d",
        baud_rate_);

      return false;
  }

  cfsetispeed(
    &tty,
    speed);

  cfsetospeed(
    &tty,
    speed);

  tty.c_cflag |=
    (CLOCAL | CREAD);

  tty.c_cflag &=
    ~CSTOPB;

  tty.c_cflag &=
    ~CRTSCTS;

  tty.c_cflag &=
    ~PARENB;

  tty.c_cflag &=
    ~CSIZE;

  tty.c_cflag |=
    CS8;

  if (
    tcsetattr(
      serial_fd_,
      TCSANOW,
      &tty) != 0)
  {
    RCLCPP_ERROR(
      rclcpp::get_logger("robot_2_hardware"),
      "tcsetattr failed: %s",
      std::strerror(errno));

    return false;
  }

  return true;
}


// ============================================================
// SERIAL WRITE
// ============================================================

bool Robot2System::writeSerial(
  const std::string & command)
{
  if (serial_fd_ < 0) {
    return false;
  }

  const char * data =
    command.c_str();

  std::size_t remaining =
    command.size();

  while (remaining > 0) {

    const ssize_t result =
      ::write(
        serial_fd_,
        data,
        remaining);

    if (result < 0) {

      if (
        errno == EAGAIN ||
        errno == EWOULDBLOCK)
      {
        continue;
      }

      RCLCPP_ERROR(
        rclcpp::get_logger("robot_2_hardware"),
        "Serial write failed: %s",
        std::strerror(errno));

      return false;
    }

    data += result;
    remaining -=
      static_cast<std::size_t>(result);
  }

  return true;
}


// ============================================================
// READ LINE
// ============================================================

bool Robot2System::readLine(
  std::string & line,
  int timeout_ms)
{
  line.clear();

  if (serial_fd_ < 0) {
    return false;
  }

  const auto start =
    std::chrono::steady_clock::now();

  while (true) {

    const auto now =
      std::chrono::steady_clock::now();

    const auto elapsed =
      std::chrono::duration_cast<
        std::chrono::milliseconds>(
          now - start).count();

    if (elapsed >= timeout_ms) {
      return false;
    }

    const int remaining_timeout =
      timeout_ms -
      static_cast<int>(elapsed);

    struct pollfd pfd {};

    pfd.fd = serial_fd_;
    pfd.events = POLLIN;

    const int result =
      poll(
        &pfd,
        1,
        remaining_timeout);

    if (result < 0) {

      if (errno == EINTR) {
        continue;
      }

      return false;
    }

    if (result == 0) {
      return false;
    }

    if (pfd.revents & POLLIN) {

      char buffer[64];

      const ssize_t bytes =
        ::read(
          serial_fd_,
          buffer,
          sizeof(buffer));

      if (bytes <= 0) {
        continue;
      }

      for (ssize_t i = 0; i < bytes; ++i) {

        const char c =
          buffer[i];

        if (c == '\n' || c == '\r') {

          if (!line.empty()) {
            return true;
          }

        } else {

          line += c;

          if (line.size() >= 256) {
            return false;
          }
        }
      }
    }
  }
}


// ============================================================
// REQUEST ENCODERS
// ============================================================

bool Robot2System::requestEncoders(
  long & left_ticks,
  long & right_ticks)
{
  if (!writeSerial("e\n")) {

    RCLCPP_ERROR(
      rclcpp::get_logger("robot_2_hardware"),
      "Failed to request encoder data");

    return false;
  }

  std::string line;

  if (!readLine(
      line,
      timeout_ms_))
  {
    RCLCPP_ERROR(
      rclcpp::get_logger("robot_2_hardware"),
      "Timed out waiting for encoder data");

    return false;
  }

  std::stringstream ss(line);

  if (!(ss >> left_ticks >> right_ticks)) {

    RCLCPP_ERROR(
      rclcpp::get_logger("robot_2_hardware"),
      "Invalid encoder response: '%s'",
      line.c_str());

    return false;
  }

  return true;
}


// ============================================================
// SEND MOTOR COMMAND
//
// Arduino expects:
//
// m <left ticks/period> <right ticks/period>
//
// ============================================================

bool Robot2System::sendMotorCommand(
  double left_velocity,
  double right_velocity)
{
  std::ostringstream command;

  command.setf(
    std::ios::fixed);

  command.precision(4);

  command
    << "m "
    << left_velocity
    << " "
    << right_velocity
    << "\n";

  return writeSerial(
    command.str());
}


// ============================================================
// VELOCITY → TICKS / PID PERIOD
// ============================================================

double Robot2System::velocityToTicksPerPeriod(
  double velocity) const
{
  const double radians_per_period =
    velocity *
    pid_period_seconds_;

  const double revolutions_per_period =
    radians_per_period /
    (2.0 * M_PI);

  return
    revolutions_per_period *
    encoder_ticks_per_rev_;
}


// ============================================================
// TICKS → RADIANS
// ============================================================

double Robot2System::ticksToRadians(
  long ticks) const
{
  return ticksToRadians(
    static_cast<double>(ticks));
}


double Robot2System::ticksToRadians(
  double ticks) const
{
  return
    ticks *
    (2.0 * M_PI) /
    encoder_ticks_per_rev_;
}


// ============================================================
// TICKS / SECOND → RAD / SECOND
// ============================================================

double Robot2System::ticksPerSecondToRadiansPerSecond(
  long delta_ticks,
  double period_seconds) const
{
  if (period_seconds <= 0.0) {
    return 0.0;
  }

  const double ticks_per_second =
    static_cast<double>(delta_ticks) /
    period_seconds;

  return
    ticks_per_second *
    (2.0 * M_PI) /
    encoder_ticks_per_rev_;
}


// ============================================================
// NUMERIC PARAMETER
// ============================================================

double Robot2System::getNumericParameter(
  const std::string & name,
  double default_value) const
{
  const auto it =
    info_.hardware_parameters.find(name);

  if (it == info_.hardware_parameters.end()) {
    return default_value;
  }

  try {

    return std::stod(it->second);

  } catch (...) {

    RCLCPP_WARN(
      rclcpp::get_logger("robot_2_hardware"),
      "Invalid numeric parameter '%s': '%s'. Using default %.3f",
      name.c_str(),
      it->second.c_str(),
      default_value);

    return default_value;
  }
}


// ============================================================
// STRING PARAMETER
// ============================================================

std::string Robot2System::getStringParameter(
  const std::string & name,
  const std::string & default_value) const
{
  const auto it =
    info_.hardware_parameters.find(name);

  if (it == info_.hardware_parameters.end()) {
    return default_value;
  }

  return it->second;
}

}  // namespace robot_2_hardware


// ============================================================
// PLUGIN EXPORT
// ============================================================

PLUGINLIB_EXPORT_CLASS(
  robot_2_hardware::Robot2System,
  hardware_interface::SystemInterface)