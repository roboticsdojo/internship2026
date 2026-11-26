#ifndef AURACLE_HARDWARE_ARDUINO_COMMS_HPP
#define AURACLE_HARDWARE_ARDUINO_COMMS_HPP

#include <libserial/SerialPort.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>

namespace auracle_hardware
{

// One "e ..." line from the Nano. See firmware serial_protocol.h.
struct Telemetry
{
  uint32_t t_ms = 0;
  long l_enc = 0;
  long r_enc = 0;
  int16_t imu[6] = {0, 0, 0, 0, 0, 0};  // ax ay az gx gy gz, raw counts
};

// One "F <L|R> <code> [speed target]" line: the Nano latched a wheel fault and stopped.
struct FaultReport
{
  char side = '?';
  int code = 0;
  bool has_detail = false;
  int speed = 0;    // filtered 1x ticks/s at the time of the fault
  int target = 0;   // commanded 1x ticks/s
};

class ArduinoComms
{
public:
  void connect(const std::string & device)
  {
    port_.Open(device);
    port_.SetBaudRate(LibSerial::BaudRate::BAUD_500000);
    port_.SetCharacterSize(LibSerial::CharacterSize::CHAR_SIZE_8);
    port_.SetParity(LibSerial::Parity::PARITY_NONE);
    port_.SetStopBits(LibSerial::StopBits::STOP_BITS_1);
    port_.SetFlowControl(LibSerial::FlowControl::FLOW_CONTROL_NONE);
    port_.FlushIOBuffers();
  }

  void disconnect()
  {
    if (port_.IsOpen()) {
      port_.Close();
    }
  }

  bool connected() const {return port_.IsOpen();}

  void sendVelocity(double left_ticks_s, double right_ticks_s)
  {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "v %.0f %.0f\n", left_ticks_s, right_ticks_s);
    write(buf);
  }

  void sendStop() {write("s\n");}

  // Opening the port resets the Nano; wait for its READY line.
  bool waitForReady(int timeout_ms)
  {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
      fill();
      size_t pos = rx_.find("READY\n");
      if (pos == std::string::npos) {
        pos = rx_.find("READY\r\n");
      }
      if (pos != std::string::npos) {
        rx_.erase(0, rx_.find('\n', pos) + 1);
        return true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
  }

  // Drains the port, keeps the newest complete telemetry line.
  bool readLatest(Telemetry & out)
  {
    fill();
    bool got = false;
    size_t start = 0;
    size_t nl;
    while ((nl = rx_.find('\n', start)) != std::string::npos) {
      rx_[nl] = '\0';  // sscanf stops here
      if (rx_[start] == 'e') {
        got |= parse(rx_.c_str() + start, out);
      } else if (rx_[start] == 'F') {
        FaultReport f;
        if (parseFault(rx_.c_str() + start, f)) {
          faults_.push_back(f);
        }
      }
      start = nl + 1;
    }
    rx_.erase(0, start);
    if (rx_.size() > 256) {
      rx_.clear();  // no newline in 256 bytes = garbage
    }
    return got;
  }

  // Faults received since the last call (the Nano sends one "F" line per fault).
  std::vector<FaultReport> takeFaults()
  {
    std::vector<FaultReport> out;
    out.swap(faults_);
    return out;
  }

private:
  static bool parseFault(const char * line, FaultReport & out)
  {
    FaultReport f;
    char side = 0;
    int n = std::sscanf(line, "F %c %d %d %d", &side, &f.code, &f.speed, &f.target);
    if (n < 2) {
      return false;
    }
    f.side = side;
    f.has_detail = (n == 4);
    out = f;
    return true;
  }

  void fill()
  {
    try {
      int n = port_.GetNumberOfBytesAvailable();
      if (n > 0) {
        std::string chunk;
        port_.Read(chunk, static_cast<size_t>(n), 5);
        rx_ += chunk;
      }
    } catch (const std::exception &) {
    }
  }

  void write(const char * line)
  {
    try {
      port_.Write(line);
    } catch (const std::exception &) {
    }
  }

  static bool parse(const char * line, Telemetry & out)
  {
    Telemetry t;
    unsigned long ms = 0;
    int n = std::sscanf(
      line, "e %lu %ld %ld %hd %hd %hd %hd %hd %hd",
      &ms, &t.l_enc, &t.r_enc,
      &t.imu[0], &t.imu[1], &t.imu[2], &t.imu[3], &t.imu[4], &t.imu[5]);
    if (n != 9) {
      return false;
    }
    t.t_ms = static_cast<uint32_t>(ms);
    out = t;
    return true;
  }

  LibSerial::SerialPort port_;
  std::string rx_;
  std::vector<FaultReport> faults_;
};

}  // namespace auracle_hardware

#endif  // AURACLE_HARDWARE_ARDUINO_COMMS_HPP