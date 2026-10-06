#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <string>

#include "behaviortree_cpp/bt_factory.h"
#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "nav2_msgs/msg/speed_limit.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"

namespace robot_2_behavior
{

// Sends a Nav2 NavigateToPose goal. RUNNING while driving, SUCCESS on arrival,
// FAILURE if rejected / aborted / canceled. Cancels the goal if halted.
// Everything runs on the main thread (the executor is spun from the tick loop),
// so no locking is needed.
class NavigateToPose : public BT::StatefulActionNode
{
public:
  using Nav = nav2_msgs::action::NavigateToPose;
  using GoalHandle = rclcpp_action::ClientGoalHandle<Nav>;

  NavigateToPose(
    const std::string & name, const BT::NodeConfig & config, rclcpp::Node::SharedPtr node)
  : BT::StatefulActionNode(name, config), node_(std::move(node))
  {
    client_ = rclcpp_action::create_client<Nav>(node_, "navigate_to_pose");
  }

  static BT::PortsList providedPorts()
  {
    return {
      BT::InputPort<double>("x", "goal x in map frame [m]"),
      BT::InputPort<double>("y", "goal y in map frame [m]"),
      BT::InputPort<double>("yaw", 0.0, "goal heading [rad]"),
      BT::OutputPort<double>("distance_remaining", "distance left along the path [m]")};
  }

  BT::NodeStatus onStart() override
  {
    double x = 0.0, y = 0.0, yaw = 0.0;
    if (!getInput("x", x) || !getInput("y", y)) {
      RCLCPP_ERROR(node_->get_logger(), "[%s] missing x/y", name().c_str());
      return BT::NodeStatus::FAILURE;
    }
    getInput("yaw", yaw);

    if (!client_->wait_for_action_server(std::chrono::seconds(2))) {
      RCLCPP_ERROR(node_->get_logger(), "navigate_to_pose action server not available");
      return BT::NodeStatus::FAILURE;
    }

    Nav::Goal goal;
    goal.pose.header.frame_id = "map";
    goal.pose.header.stamp = node_->now();
    goal.pose.pose.position.x = x;
    goal.pose.pose.position.y = y;
    goal.pose.pose.orientation.z = std::sin(yaw / 2.0);
    goal.pose.pose.orientation.w = std::cos(yaw / 2.0);

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
    RCLCPP_INFO(node_->get_logger(), "[%s] goal -> (%.2f, %.2f, %.2f rad)", name().c_str(), x, y, yaw);
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
  rclcpp_action::Client<Nav>::SharedPtr client_;
  GoalHandle::SharedPtr handle_;
  uint64_t goal_id_{0};
  bool done_{false}, rejected_{false}, cancel_requested_{false};
  rclcpp_action::ResultCode code_{rclcpp_action::ResultCode::UNKNOWN};
  double distance_{-1.0};
};

// Runs in parallel with NavigateToPose. Every tick it works out the speed needed to
// finish inside the time limit and publishes it on Nav2's `speed_limit` topic:
//   behind schedule -> limit goes up (clamped to max_speed)
//   ahead of schedule -> limit goes down (clamped to min_speed)
// Returns FAILURE when the time limit expires (the Parallel then cancels navigation).
// Always resets Nav2's speed limit when it stops.
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
      return BT::NodeStatus::RUNNING;  // no feedback yet
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
    msg.speed_limit = v;  // 0.0 = no limit (Nav2 restores its default)
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

}  // namespace robot_2_behavior