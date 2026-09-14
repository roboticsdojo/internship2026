#ifndef AURACLE_HARDWARE_ARDUINO_COMMS_HPP
#define AURACLE_HARDWARE_ARDUINO_COMMS_HPP

#include <libserial/SerialPort.h>
#include <string>
#include <sstream>
#include <cstdlib>
#include <chrono>
#include <rclcpp/rclcpp.hpp>

namespace auracle_hardware
{

// Wraps the LibSerial connection to the Arduino Nano and speaks the exact
// wire protocol implemented in firmware/serial_protocol.cpp:
//   ROS  -> Arduino : "v <left> <right>\n"                 (target wheel vel)
//   Arduino -> ROS  : "e <l_enc> <r_enc> i <ax> <ay> <az> <gx> <gy> <gz>\n"
class ArduinoComms
{
public:
  ArduinoComms() = default;

  void connect(const std::string & serial_device, int32_t baud_rate, int32_t timeout_ms)
  {
    timeout_ms_ = timeout_ms;
    serial_conn_.Open(serial_device);
    serial_conn_.SetBaudRate(convertBaudRate(baud_rate));
  }

  void disconnect()
  {
    if (serial_conn_.IsOpen()) {
      serial_conn_.Close();
    }
  }

  bool connected() const
  {
    return serial_conn_.IsOpen();
  }

  // Sends the velocity command line. Non-blocking on the write side.
  void sendVelocity(double left_rad_s, double right_rad_s)
  {
    std::stringstream ss;
    ss << "v " << left_rad_s << " " << right_rad_s << "\n";
    try {
      serial_conn_.Write(ss.str());
    } catch (const std::exception & e) {
      RCLCPP_WARN(rclcpp::get_logger("ArduinoComms"), "Failed to write to serial: %s", e.what());
    }
  }

  // Reads one line of telemetry. Returns false (leaving outputs untouched)
  // if no complete line was available within timeout_ms_ - this must never
  // block the ros2_control read() cycle indefinitely.
  bool readTelemetry(long & l_enc, long & r_enc, float * acc, float * gyro)
  {
    std::string line;
    try {
      serial_conn_.ReadLine(line, '\n', timeout_ms_);
    } catch (const LibSerial::ReadTimeout &) {
      return false;
    } catch (const std::exception & e) {
      RCLCPP_WARN(rclcpp::get_logger("ArduinoComms"), "Serial read error: %s", e.what());
      return false;
    }

    if (line.empty() || line[0] != 'e') {
      return false;
    }

    int matched = std::sscanf(
      line.c_str(), "e %ld %ld i %f %f %f %f %f %f",
      &l_enc, &r_enc, &acc[0], &acc[1], &acc[2], &gyro[0], &gyro[1], &gyro[2]);

    return matched == 8;
  }

  // Handshake: waits (bounded) for the firmware's "READY" line after reset.
  bool waitForReady(int timeout_ms)
  {
    auto start = std::chrono::steady_clock::now();
    std::string line;
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - start).count() < timeout_ms)
    {
      try {
        serial_conn_.ReadLine(line, '\n', 200);
      } catch (const LibSerial::ReadTimeout &) {
        continue;
      } catch (const std::exception &) {
        continue;
      }
      if (line.rfind("READY", 0) == 0) {
        return true;
      }
    }
    return false;
  }

private:
  static LibSerial::BaudRate convertBaudRate(int32_t baud_rate)
  {
    switch (baud_rate) {
      case 9600: return LibSerial::BaudRate::BAUD_9600;
      case 19200: return LibSerial::BaudRate::BAUD_19200;
      case 38400: return LibSerial::BaudRate::BAUD_38400;
      case 57600: return LibSerial::BaudRate::BAUD_57600;
      case 115200: return LibSerial::BaudRate::BAUD_115200;
      default:
        RCLCPP_WARN(
          rclcpp::get_logger("ArduinoComms"),
          "Unsupported baud rate %d, defaulting to 115200", baud_rate);
        return LibSerial::BaudRate::BAUD_115200;
    }
  }

  LibSerial::SerialPort serial_conn_;
  int32_t timeout_ms_ = 100;
};

}  // namespace auracle_hardware

#endif  // AURACLE_HARDWARE_ARDUINO_COMMS_HPP
