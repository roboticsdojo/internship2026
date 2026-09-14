#ifndef AURACLE_HARDWARE_WHEEL_HPP
#define AURACLE_HARDWARE_WHEEL_HPP

#include <string>
#include <cmath>

namespace auracle_hardware
{

// Holds the ROS-facing state for a single wheel joint and converts between
// raw encoder ticks (what the Arduino sends) and radians (what ros2_control
// state interfaces expect).
class Wheel
{
public:
  std::string name = "";
  double position = 0.0;      // rad
  double velocity = 0.0;      // rad/s
  double command = 0.0;       // rad/s, commanded velocity from the diff_drive_controller
  long enc_ticks = 0;         // last raw encoder tick count received
  double ticks_per_rev = 0.0;

  Wheel() = default;

  void setup(const std::string & wheel_name, double counts_per_rev)
  {
    name = wheel_name;
    ticks_per_rev = counts_per_rev;
  }

  double ticksToRadians(long ticks) const
  {
    if (ticks_per_rev == 0.0) {
      return 0.0;
    }
    return (static_cast<double>(ticks) / ticks_per_rev) * 2.0 * M_PI;
  }
};

}  // namespace auracle_hardware

#endif  // AURACLE_HARDWARE_WHEEL_HPP
