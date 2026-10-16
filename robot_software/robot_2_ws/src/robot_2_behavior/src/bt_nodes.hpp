#pragma once

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <map>
#include <memory>
#include <string>

#include "behaviortree_cpp/bt_factory.h"
#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "nav2_msgs/msg/speed_limit.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "std_msgs/msg/float64.hpp"
#include "std_msgs/msg/int32.hpp"

namespace robot_2_behavior
{

using SteadyClock = std::chrono::steady_clock;

static inline double secondsSince(SteadyClock::time_point t0)
{
  return std::chrono::duration<double>(SteadyClock::now() - t0).count();
}

// ---------------------------------------------------------------------------
// Waypoints (name -> pose in the map frame), loaded from a ROS parameter file
// ---------------------------------------------------------------------------
struct Pose2D
{
  double x{0.0}, y{0.0}, yaw{0.0};
};
using WaypointMap = std::map<std::string, Pose2D>;

// ---------------------------------------------------------------------------
// NavigateToPose: drive to a NAMED waypoint with Nav2.
// RUNNING while driving, SUCCESS on arrival, FAILURE if rejected / aborted /
// canceled / waypoint not defined. Cancels the Nav2 goal if halted.
// Everything runs on the main thread (executor is spun from the tick loop).
// ---------------------------------------------------------------------------
class NavigateToPose : public BT::StatefulActionNode
{
public:
  using Nav = nav2_msgs::action::NavigateToPose;
  using GoalHandle = rclcpp_action::ClientGoalHandle<Nav>;

  NavigateToPose(
    const std::string & name, const BT::NodeConfig & config, rclcpp::Node::SharedPtr node,
    std::shared_ptr<const WaypointMap> waypoints)
  : BT::StatefulActionNode(name, config), node_(std::move(node)), waypoints_(std::move(waypoints))
  {
    client_ = rclcpp_action::create_client<Nav>(node_, "navigate_to_pose");
  }

  static BT::PortsList providedPorts()
  {
    return {
      BT::InputPort<std::string>("waypoint", "waypoint name from the waypoints file, e.g. A"),
      BT::OutputPort<double>("distance_remaining", "distance left along the path [m]")};
  }

  BT::NodeStatus onStart() override
  {
    std::string wp;
    if (!getInput("waypoint", wp)) {
      RCLCPP_ERROR(node_->get_logger(), "[%s] missing 'waypoint' port", name().c_str());
      return BT::NodeStatus::FAILURE;
    }
    const auto it = waypoints_->find(wp);
    if (it == waypoints_->end()) {
      RCLCPP_ERROR(
        node_->get_logger(), "[%s] waypoint '%s' is not defined in the waypoints file",
        name().c_str(), wp.c_str());
      return BT::NodeStatus::FAILURE;
    }
    const Pose2D & p = it->second;

    if (!client_->wait_for_action_server(std::chrono::seconds(2))) {
      RCLCPP_ERROR(node_->get_logger(), "navigate_to_pose action server not available");
      return BT::NodeStatus::FAILURE;
    }

    Nav::Goal goal;
    goal.pose.header.frame_id = "map";
    goal.pose.header.stamp = node_->now();
    goal.pose.pose.position.x = p.x;
    goal.pose.pose.position.y = p.y;
    goal.pose.pose.orientation.z = std::sin(p.yaw / 2.0);
    goal.pose.pose.orientation.w = std::cos(p.yaw / 2.0);

    const auto id = ++goal_id_;
    handle_.reset();
    done_ = false;
    rejected_ = false;
    cancel_requested_ = false;
    code_ = rclcpp_action::ResultCode::UNKNOWN;
    distance_ = -1.0;
    setOutput("distance_remaining", distance_);

    rclcpp_action::Client<Nav>::SendGoalOptions opts;
    opts.goal_response_callback = [this, id](GoalHandle::SharedPtr gh) {
      if (id != goal_id_) {return;}
      if (!gh) {
        rejected_ = true;
        return;
      }
      handle_ = gh;
      if (cancel_requested_) {client_->async_cancel_goal(handle_);}
    };
    opts.feedback_callback =
      [this, id](GoalHandle::SharedPtr, const std::shared_ptr<const Nav::Feedback> fb) {
        if (id == goal_id_) {distance_ = fb->distance_remaining;}
      };
    opts.result_callback = [this, id](const GoalHandle::WrappedResult & res) {
      if (id != goal_id_) {return;}
      code_ = res.code;
      done_ = true;
    };

    client_->async_send_goal(goal, opts);
    RCLCPP_INFO(
      node_->get_logger(), "[%s] -> waypoint %s (%.2f, %.2f, %.2f rad)",
      name().c_str(), wp.c_str(), p.x, p.y, p.yaw);
    return BT::NodeStatus::RUNNING;
  }

  BT::NodeStatus onRunning() override
  {
    setOutput("distance_remaining", distance_);
    if (rejected_) {
      RCLCPP_WARN(node_->get_logger(), "[%s] goal rejected", name().c_str());
      return BT::NodeStatus::FAILURE;
    }
    if (done_) {
      const bool ok = (code_ == rclcpp_action::ResultCode::SUCCEEDED);
      RCLCPP_INFO(node_->get_logger(), "[%s] %s", name().c_str(), ok ? "arrived" : "failed");
      return ok ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    }
    return BT::NodeStatus::RUNNING;
  }

  void onHalted() override
  {
    cancel_requested_ = true;
    if (handle_ && !done_) {client_->async_cancel_goal(handle_);}
  }

private:
  rclcpp::Node::SharedPtr node_;
  std::shared_ptr<const WaypointMap> waypoints_;
  rclcpp_action::Client<Nav>::SharedPtr client_;
  GoalHandle::SharedPtr handle_;
  uint64_t goal_id_{0};
  bool done_{false}, rejected_{false}, cancel_requested_{false};
  rclcpp_action::ResultCode code_{rclcpp_action::ResultCode::UNKNOWN};
  double distance_{-1.0};
};

// ---------------------------------------------------------------------------
// SpeedAdapter (optional, not used by the current mission tree): raises/lowers
// Nav2's speed_limit so a leg finishes within time_limit. Run it in a Parallel
// next to NavigateToPose (see the earlier timed-leg example).
// ---------------------------------------------------------------------------
class SpeedAdapter : public BT::StatefulActionNode
{
public:
  SpeedAdapter(
    const std::string & name, const BT::NodeConfig & config, rclcpp::Node::SharedPtr node)
  : BT::StatefulActionNode(name, config), node_(std::move(node))
  {
    pub_ = node_->create_publisher<nav2_msgs::msg::SpeedLimit>("speed_limit", rclcpp::QoS(10));
  }

  static BT::PortsList providedPorts()
  {
    return {
      BT::InputPort<double>("distance_remaining", "from NavigateToPose [m]"),
      BT::InputPort<double>("time_limit", "seconds allowed for this leg"),
      BT::InputPort<double>("min_speed", 0.1, "lowest speed limit to command [m/s]"),
      BT::InputPort<double>("max_speed", 0.5, "highest speed limit to command [m/s]"),
      BT::InputPort<double>("margin", 1.15, "safety factor on the required speed")};
  }

  BT::NodeStatus onStart() override
  {
    double limit = 0.0;
    if (!getInput("time_limit", limit) || limit <= 0.0) {
      RCLCPP_ERROR(node_->get_logger(), "[%s] bad time_limit", name().c_str());
      return BT::NodeStatus::FAILURE;
    }
    getInput("min_speed", min_);
    getInput("max_speed", max_);
    getInput("margin", margin_);
    deadline_ = node_->now() + rclcpp::Duration::from_seconds(limit);
    last_ = -1.0;
    return BT::NodeStatus::RUNNING;
  }

  BT::NodeStatus onRunning() override
  {
    const double left = (deadline_ - node_->now()).seconds();
    if (left <= 0.0) {
      RCLCPP_WARN(node_->get_logger(), "[%s] time limit expired", name().c_str());
      reset();
      return BT::NodeStatus::FAILURE;
    }
    double dist = -1.0;
    if (!getInput("distance_remaining", dist) || dist < 0.0) {
      return BT::NodeStatus::RUNNING;
    }
    const double required = margin_ * dist / left;
    if (required > max_) {
      RCLCPP_WARN_THROTTLE(
        node_->get_logger(), *node_->get_clock(), 2000,
        "[%s] need %.2f m/s but max is %.2f m/s - deadline at risk",
        name().c_str(), required, max_);
    }
    const double v = std::clamp(required, min_, max_);
    if (std::fabs(v - last_) > 0.02) {
      publish(v);
      last_ = v;
    }
    return BT::NodeStatus::RUNNING;
  }

  void onHalted() override {reset();}

private:
  void publish(double v)
  {
    nav2_msgs::msg::SpeedLimit msg;
    msg.header.stamp = node_->now();
    msg.percentage = false;
    msg.speed_limit = v;  // 0.0 = no limit
    pub_->publish(msg);
  }
  void reset()
  {
    publish(0.0);
    last_ = -1.0;
  }

  rclcpp::Node::SharedPtr node_;
  rclcpp::Publisher<nav2_msgs::msg::SpeedLimit>::SharedPtr pub_;
  rclcpp::Time deadline_{0, 0, RCL_ROS_TIME};
  double min_{0.1}, max_{0.5}, margin_{1.15}, last_{-1.0};
};

// ---------------------------------------------------------------------------
// Process management: start/stop other ROS launch files (camera, detectors)
// from the tree. Each process gets its own process group so SIGINT reaches
// every child of `ros2 launch`.
// ---------------------------------------------------------------------------
class ProcessRegistry
{
public:
  static ProcessRegistry & instance()
  {
    static ProcessRegistry r;
    return r;
  }

  bool start(const std::string & id, const std::string & command)
  {
    stop(id);
    const pid_t pid = fork();
    if (pid < 0) {return false;}
    if (pid == 0) {
      setpgid(0, 0);
      execl("/bin/bash", "bash", "-c", command.c_str(), static_cast<char *>(nullptr));
      _exit(127);
    }
    setpgid(pid, pid);
    pids_[id] = pid;
    return true;
  }

  bool alive(const std::string & id)
  {
    const auto it = pids_.find(id);
    if (it == pids_.end()) {return false;}
    int status = 0;
    if (waitpid(it->second, &status, WNOHANG) == 0) {return true;}
    pids_.erase(it);
    return false;
  }

  void stop(const std::string & id)
  {
    const auto it = pids_.find(id);
    if (it == pids_.end()) {return;}
    const pid_t pid = it->second;
    killpg(pid, SIGINT);
    for (int i = 0; i < 50; ++i) {  // up to 5 s for a clean shutdown
      int status = 0;
      if (waitpid(pid, &status, WNOHANG) != 0) {
        pids_.erase(id);
        return;
      }
      usleep(100000);
    }
    killpg(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
    pids_.erase(id);
  }

  void stopAll()
  {
    while (!pids_.empty()) {stop(pids_.begin()->first);}
  }

private:
  std::map<std::string, pid_t> pids_;
};

// Starts `command` in the background. SUCCESS once it has stayed alive for
// settle_ms; FAILURE if it exits before that (e.g. hailo device missing).
class LaunchProcess : public BT::StatefulActionNode
{
public:
  LaunchProcess(const std::string & name, const BT::NodeConfig & config)
  : BT::StatefulActionNode(name, config) {}

  static BT::PortsList providedPorts()
  {
    return {
      BT::InputPort<std::string>("id", "unique name for this process"),
      BT::InputPort<std::string>("command", "shell command, e.g. ros2 launch pkg file.launch.py"),
      BT::InputPort<int>("settle_ms", 3000, "must stay alive this long to count as started")};
  }

  BT::NodeStatus onStart() override
  {
    std::string command;
    if (!getInput("id", id_) || !getInput("command", command)) {
      return BT::NodeStatus::FAILURE;
    }
    int settle = 3000;
    getInput("settle_ms", settle);
    settle_s_ = settle / 1000.0;
    if (!ProcessRegistry::instance().start(id_, command)) {return BT::NodeStatus::FAILURE;}
    t0_ = SteadyClock::now();
    return BT::NodeStatus::RUNNING;
  }

  BT::NodeStatus onRunning() override
  {
    if (!ProcessRegistry::instance().alive(id_)) {return BT::NodeStatus::FAILURE;}
    return secondsSince(t0_) >= settle_s_ ? BT::NodeStatus::SUCCESS : BT::NodeStatus::RUNNING;
  }

  void onHalted() override {ProcessRegistry::instance().stop(id_);}

private:
  std::string id_;
  double settle_s_{3.0};
  SteadyClock::time_point t0_;
};

// Stops a process started by LaunchProcess. Always SUCCESS (even if not running).
class KillProcess : public BT::SyncActionNode
{
public:
  KillProcess(const std::string & name, const BT::NodeConfig & config)
  : BT::SyncActionNode(name, config) {}

  static BT::PortsList providedPorts()
  {
    return {BT::InputPort<std::string>("id", "name given to LaunchProcess")};
  }

  BT::NodeStatus tick() override
  {
    std::string id;
    if (getInput("id", id)) {ProcessRegistry::instance().stop(id);}
    return BT::NodeStatus::SUCCESS;
  }
};

// ---------------------------------------------------------------------------
// ReadWasteClass: waits for the detector to report the same class `confirm`
// times in a row. 0 = metallic, 1 = organic, 2 = plastic.
// Writes waste_type = "metallic" | "organic" | "plastic".
//
// ASSUMPTION (adapt to your detector): it publishes std_msgs/Int32 with the
// class id on `topic`. If your detector publishes something else (e.g.
// vision_msgs/Detection2DArray), only the subscription + callback below change.
// ---------------------------------------------------------------------------
class ReadWasteClass : public BT::StatefulActionNode
{
public:
  ReadWasteClass(
    const std::string & name, const BT::NodeConfig & config, rclcpp::Node::SharedPtr node)
  : BT::StatefulActionNode(name, config), node_(std::move(node)) {}

  static BT::PortsList providedPorts()
  {
    return {
      BT::InputPort<std::string>("topic", "/waste_class", "detector output topic"),
      BT::InputPort<int>("confirm", 5, "same class this many times in a row"),
      BT::InputPort<int>("timeout_ms", 10000, "fail if not confirmed within this time"),
      BT::OutputPort<std::string>("waste_type", "metallic | organic | plastic")};
  }

  BT::NodeStatus onStart() override
  {
    std::string topic = "/waste_class";
    getInput("topic", topic);
    if (!sub_ || topic != topic_) {
      topic_ = topic;
      sub_ = node_->create_subscription<std_msgs::msg::Int32>(
        topic_, rclcpp::QoS(10), [this](std_msgs::msg::Int32::SharedPtr msg) {
          if (!listening_) {return;}
          if (msg->data < 0 || msg->data > 2) {
            streak_ = 0;
            return;
          }
          if (msg->data == last_) {
            ++streak_;
          } else {
            last_ = msg->data;
            streak_ = 1;
          }
        });
    }
    int confirm = 5, timeout = 10000;
    getInput("confirm", confirm);
    getInput("timeout_ms", timeout);
    confirm_ = confirm;
    timeout_s_ = timeout / 1000.0;
    last_ = -1;
    streak_ = 0;
    listening_ = true;
    t0_ = SteadyClock::now();
    return BT::NodeStatus::RUNNING;
  }

  BT::NodeStatus onRunning() override
  {
    if (streak_ >= confirm_) {
      static const char * names[] = {"metallic", "organic", "plastic"};
      listening_ = false;
      setOutput("waste_type", std::string(names[last_]));
      RCLCPP_INFO(node_->get_logger(), "[%s] detected: %s (class %d)", name().c_str(), names[last_], last_);
      return BT::NodeStatus::SUCCESS;
    }
    if (secondsSince(t0_) > timeout_s_) {
      listening_ = false;
      RCLCPP_WARN(node_->get_logger(), "[%s] no confirmed class on %s", name().c_str(), topic_.c_str());
      return BT::NodeStatus::FAILURE;
    }
    return BT::NodeStatus::RUNNING;
  }

  void onHalted() override {listening_ = false;}

private:
  rclcpp::Node::SharedPtr node_;
  rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr sub_;
  std::string topic_;
  bool listening_{false};
  int last_{-1}, streak_{0}, confirm_{5};
  double timeout_s_{10.0};
  SteadyClock::time_point t0_;
};

// ---------------------------------------------------------------------------
// OpenServo: publishes open_angle, holds for hold_ms, publishes close_angle.
//
// ASSUMPTION (adapt to your robot): the offload servo is commanded with a
// std_msgs/Float64 angle on `topic`. Change the publisher type/topic once you
// know how the servo is exposed in ROS.
// ---------------------------------------------------------------------------
class OpenServo : public BT::StatefulActionNode
{
public:
  OpenServo(const std::string & name, const BT::NodeConfig & config, rclcpp::Node::SharedPtr node)
  : BT::StatefulActionNode(name, config), node_(std::move(node)) {}

  static BT::PortsList providedPorts()
  {
    return {
      BT::InputPort<std::string>("topic", "/robot_2/offload_servo", "servo command topic"),
      BT::InputPort<double>("open_angle", 90.0, "angle that offloads the waste [deg]"),
      BT::InputPort<double>("close_angle", 0.0, "angle to return to [deg]"),
      BT::InputPort<int>("hold_ms", 2000, "how long to stay open")};
  }

  BT::NodeStatus onStart() override
  {
    std::string topic = "/robot_2/offload_servo";
    getInput("topic", topic);
    if (!pub_ || topic != topic_) {
      topic_ = topic;
      pub_ = node_->create_publisher<std_msgs::msg::Float64>(topic_, rclcpp::QoS(10));
    }
    getInput("open_angle", open_);
    getInput("close_angle", close_);
    int hold = 2000;
    getInput("hold_ms", hold);
    hold_s_ = hold / 1000.0;
    opened_ = false;
    t0_ = SteadyClock::now();
    return BT::NodeStatus::RUNNING;
  }

  BT::NodeStatus onRunning() override
  {
    constexpr double kSettle = 0.3;  // let the publisher be discovered
    const double t = secondsSince(t0_);
    if (!opened_ && t >= kSettle) {
      send(open_);
      opened_ = true;
      RCLCPP_INFO(node_->get_logger(), "[%s] servo open (%.0f)", name().c_str(), open_);
    }
    if (opened_ && t >= kSettle + hold_s_) {
      send(close_);
      RCLCPP_INFO(node_->get_logger(), "[%s] servo closed (%.0f)", name().c_str(), close_);
      return BT::NodeStatus::SUCCESS;
    }
    return BT::NodeStatus::RUNNING;
  }

  void onHalted() override
  {
    if (opened_) {send(close_);}
  }

private:
  void send(double angle)
  {
    std_msgs::msg::Float64 msg;
    msg.data = angle;
    pub_->publish(msg);
  }

  rclcpp::Node::SharedPtr node_;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr pub_;
  std::string topic_;
  double open_{90.0}, close_{0.0}, hold_s_{2.0};
  bool opened_{false};
  SteadyClock::time_point t0_;
};

}  // namespace robot_2_behavior