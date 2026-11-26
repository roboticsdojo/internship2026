// auracle_bt mission runner.
//
// Sits above Nav2: it sends goals to the NavigateThroughPoses action, it does not drive.
// Default tree (behavior_trees/region_tree.xml): wait for Nav2 -> set the start pose from `start` in
// regions.yaml (robot parked where mapping began) -> then forever: wait for a region number N ->
// drive to LOAD N, pause, drive to OFFLOAD N, drive HOME, turn to the start heading, wait for the next N.
// Region numbers come from the launch argument route:=N (first one) and from the topic `region`:
//   ros2 topic pub --once /auracle/region std_msgs/msg/Int32 "{data: 2}"
// The regions live in config/regions.yaml. Fill it once with mark:=true (mark_tree.xml): you click the
// centre of each region with RViz "Publish Point" and the runner writes the file.
// Other trees (pass tree_file, a bare name is looked up in behavior_trees/):
//   region_click_tree.xml same loop, but the start pose is a 2D Pose Estimate click in RViz (auto_pose:=false)
//   mark_tree.xml    record the six regions from Publish Point clicks (mark:=true selects it)
//   goal_tree.xml    2D Pose Estimate, then drive to every 2D Goal Pose you click
//   vision_tree.xml  same loop as region_tree, but the region comes from the camera (vision:=true):
//                    WaitForDetection asks auracle_vision for a waste class and maps it to a region
//   vision_click_tree.xml  vision_tree with a 2D Pose Estimate click (vision:=true auto_pose:=false)
//
// Run with the same namespace as Nav2 (launch/mission.launch.py, namespace:=auracle), so the
// relative names below become /auracle/navigate_to_pose, /auracle/initialpose, /auracle/mission_goal.
// RViz's 2D Goal Pose must publish to mission_goal (laptop: goal_topic:=mission_goal), NOT goal_pose:
// Nav2's own bt_navigator listens on goal_pose and would drive there by itself, ignoring this tree.
#include <behaviortree_cpp/bt_factory.h>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/int32.hpp>
#include <std_msgs/msg/string.hpp>
#include <geometry_msgs/msg/point_stamped.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <nav2_msgs/action/navigate_through_poses.hpp>
#include <nav2_msgs/action/navigate_to_pose.hpp>
#include <nav2_msgs/action/spin.hpp>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <ament_index_cpp/get_package_share_directory.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <deque>
#include <fstream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>

using Poses = std::vector<geometry_msgs::msg::PoseStamped>;
using Nav = nav2_msgs::action::NavigateThroughPoses;
using Nav1 = nav2_msgs::action::NavigateToPose;
using Spin = nav2_msgs::action::Spin;

// Things the ROS callbacks in main() tell the tree nodes.
struct Shared {
  std::string label;        // vision label (sorting tree only)
  bool have_amcl{false};    // AMCL has published a pose: it is up
  bool have_pose_estimate{false};   // a 2D Pose Estimate arrived on initialpose
  bool accepting_goal{false};       // WaitForGoal is running: a clicked goal is taken
  bool goal_ready{false};
  geometry_msgs::msg::PoseStamped goal;
  std::deque<int> regions;          // region numbers waiting to be run (route:=N, topic `region`)
  bool recording{false};            // RecordRegions is running: Publish Point clicks are kept
  std::vector<geometry_msgs::msg::PointStamped> clicks;
  bool accepting_vision{false};     // WaitForDetection is running: a vision/decision is taken
  std::string vision_label;         // that decision ("metal", "organic", "plastic")
};

static std::string share_dir(const std::string & pkg) {
  return ament_index_cpp::get_package_share_directory(pkg);
}

// Wait for the vision detector, store the label (sorting tree only)
class GetDetectedClass : public BT::StatefulActionNode {
public:
  GetDetectedClass(const std::string & n, const BT::NodeConfig & c, std::shared_ptr<Shared> s)
  : BT::StatefulActionNode(n, c), state_(s) {}
  static BT::PortsList providedPorts() {
    return {BT::OutputPort<std::string>("detected_class")};
  }
  BT::NodeStatus onStart() override { return check(); }
  BT::NodeStatus onRunning() override { return check(); }
  void onHalted() override {}
private:
  BT::NodeStatus check() {
    if (state_->label.empty()) return BT::NodeStatus::RUNNING;
    std::string l = state_->label;
    std::transform(l.begin(), l.end(), l.begin(), ::toupper);
    setOutput("detected_class", l);
    return BT::NodeStatus::SUCCESS;
  }
  std::shared_ptr<Shared> state_;
};

// Wait until Nav2's action server is up AND AMCL has published (so the map frame exists).
class WaitForNav2 : public BT::StatefulActionNode {
public:
  WaitForNav2(const std::string & n, const BT::NodeConfig & c,
              rclcpp::Node::SharedPtr node, std::shared_ptr<Shared> s)
  : BT::StatefulActionNode(n, c), node_(node), state_(s) {
    client_ = rclcpp_action::create_client<Nav>(node_, "navigate_through_poses");
  }
  static BT::PortsList providedPorts() {
    // false = only wait for the action server (AMCL has no pose yet when you still have to give one)
    return {BT::InputPort<bool>("need_amcl", true, "also wait for AMCL's first pose")};
  }
  BT::NodeStatus onStart() override {
    need_amcl_ = true;
    getInput("need_amcl", need_amcl_);
    t0_ = node_->now();
    last_log_ = t0_;
    return check();
  }
  BT::NodeStatus onRunning() override { return check(); }
  void onHalted() override {}
private:
  BT::NodeStatus check() {
    const bool server = client_->action_server_is_ready();
    if (server && (state_->have_amcl || !need_amcl_)) {
      RCLCPP_INFO(node_->get_logger(), need_amcl_ ? "Nav2 and AMCL are up" : "Nav2 is up");
      return BT::NodeStatus::SUCCESS;
    }
    if ((node_->now() - last_log_).seconds() > 5.0) {
      last_log_ = node_->now();
      RCLCPP_INFO(node_->get_logger(), "waiting for Nav2 (%s) and AMCL (%s) ...",
                  server ? "up" : "not yet", state_->have_amcl ? "up" : "not yet");
    }
    return BT::NodeStatus::RUNNING;
  }
  rclcpp::Node::SharedPtr node_;
  std::shared_ptr<Shared> state_;
  rclcpp_action::Client<Nav>::SharedPtr client_;
  rclcpp::Time t0_, last_log_;
  bool need_amcl_{true};
};

// Ask for a 2D Pose Estimate and wait until one arrives (RViz publishes it on initialpose, AMCL
// reads the same message). Holds 2.5 s afterwards so AMCL has applied it before any goal is sent.
class WaitForInitialPose : public BT::StatefulActionNode {
public:
  WaitForInitialPose(const std::string & n, const BT::NodeConfig & c,
                     rclcpp::Node::SharedPtr node, std::shared_ptr<Shared> s)
  : BT::StatefulActionNode(n, c), node_(node), state_(s) {}
  static BT::PortsList providedPorts() { return {}; }
  BT::NodeStatus onStart() override {
    state_->have_pose_estimate = false;
    got_ = false;
    last_log_ = node_->now();
    RCLCPP_INFO(node_->get_logger(),
      "Set the robot's position: RViz 2D Pose Estimate, click where it is and drag the way it faces");
    return BT::NodeStatus::RUNNING;
  }
  BT::NodeStatus onRunning() override {
    if (!got_) {
      if (state_->have_pose_estimate) {
        got_ = true;
        t0_ = node_->now();
        RCLCPP_INFO(node_->get_logger(), "pose estimate received");
      } else {
        if ((node_->now() - last_log_).seconds() > 10.0) {
          last_log_ = node_->now();
          RCLCPP_INFO(node_->get_logger(), "still waiting for the 2D Pose Estimate in RViz ...");
        }
        return BT::NodeStatus::RUNNING;
      }
    }
    return (node_->now() - t0_).seconds() > 2.5 ? BT::NodeStatus::SUCCESS : BT::NodeStatus::RUNNING;
  }
  void onHalted() override {}
private:
  rclcpp::Node::SharedPtr node_;
  std::shared_ptr<Shared> state_;
  rclcpp::Time t0_, last_log_;
  bool got_{false};
};

// Wait for a 2D Goal Pose click (topic mission_goal) and put it on the blackboard.
class WaitForGoal : public BT::StatefulActionNode {
public:
  WaitForGoal(const std::string & n, const BT::NodeConfig & c,
              rclcpp::Node::SharedPtr node, std::shared_ptr<Shared> s)
  : BT::StatefulActionNode(n, c), node_(node), state_(s) {}
  static BT::PortsList providedPorts() {
    return {BT::OutputPort<geometry_msgs::msg::PoseStamped>("goal")};
  }
  BT::NodeStatus onStart() override {
    state_->goal_ready = false;
    state_->accepting_goal = true;
    RCLCPP_INFO(node_->get_logger(), "Ready: click a 2D Goal Pose in RViz (the target region)");
    return check();
  }
  BT::NodeStatus onRunning() override { return check(); }
  void onHalted() override { state_->accepting_goal = false; }
private:
  BT::NodeStatus check() {
    if (!state_->goal_ready) return BT::NodeStatus::RUNNING;
    state_->accepting_goal = false;
    state_->goal_ready = false;
    setOutput("goal", state_->goal);
    return BT::NodeStatus::SUCCESS;
  }
  rclcpp::Node::SharedPtr node_;
  std::shared_ptr<Shared> state_;
};

// Drive to one goal with Nav2 (NavigateToPose).
class NavigateToGoal : public BT::StatefulActionNode {
  using GH = rclcpp_action::ClientGoalHandle<Nav1>;
public:
  NavigateToGoal(const std::string & n, const BT::NodeConfig & c, rclcpp::Node::SharedPtr node)
  : BT::StatefulActionNode(n, c), node_(node) {
    client_ = rclcpp_action::create_client<Nav1>(node_, "navigate_to_pose");
  }
  static BT::PortsList providedPorts() {
    return {BT::InputPort<geometry_msgs::msg::PoseStamped>("goal")};
  }
  BT::NodeStatus onStart() override {
    geometry_msgs::msg::PoseStamped goal;
    if (!getInput("goal", goal)) return BT::NodeStatus::FAILURE;
    if (!client_->wait_for_action_server(std::chrono::seconds(5))) {
      RCLCPP_ERROR(node_->get_logger(), "navigate_to_pose is not available");
      return BT::NodeStatus::FAILURE;
    }
    Nav1::Goal g;
    g.pose = goal;
    g.pose.header.frame_id = "map";
    g.pose.header.stamp = rclcpp::Time(0, 0, node_->get_clock()->get_clock_type());   // latest tf
    done_ = false; ok_ = false; handle_.reset();
    rclcpp_action::Client<Nav1>::SendGoalOptions o;
    o.goal_response_callback = [this](GH::SharedPtr h) {
      if (!h) {
        RCLCPP_ERROR(node_->get_logger(), "Nav2 rejected the goal");
        done_ = true; ok_ = false;
      } else { handle_ = h; } };
    o.result_callback = [this](const GH::WrappedResult & r) {
      done_ = true;
      ok_ = (r.code == rclcpp_action::ResultCode::SUCCEEDED);
      RCLCPP_INFO(node_->get_logger(), "goal finished: %s", ok_ ? "reached" : "not reached"); };
    client_->async_send_goal(g, o);
    RCLCPP_INFO(node_->get_logger(), "driving to (%.2f, %.2f)", goal.pose.position.x, goal.pose.position.y);
    return BT::NodeStatus::RUNNING;
  }
  BT::NodeStatus onRunning() override {
    if (!done_) return BT::NodeStatus::RUNNING;
    return ok_ ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
  }
  void onHalted() override { if (handle_) client_->async_cancel_goal(handle_); }
private:
  rclcpp::Node::SharedPtr node_;
  rclcpp_action::Client<Nav1>::SharedPtr client_;
  GH::SharedPtr handle_;
  bool done_{false}, ok_{false};
};

// Tell AMCL where the robot is parked ("start" in the route file), like 2D Pose Estimate in RViz.
// Holds for a couple of seconds so AMCL has applied it before the first goal.
class SetInitialPose : public BT::StatefulActionNode {
public:
  SetInitialPose(const std::string & n, const BT::NodeConfig & c, rclcpp::Node::SharedPtr node)
  : BT::StatefulActionNode(n, c), node_(node) {
    pub_ = node_->create_publisher<geometry_msgs::msg::PoseWithCovarianceStamped>("initialpose", 10);
  }
  static BT::PortsList providedPorts() { return {BT::InputPort<std::string>("file")}; }

  BT::NodeStatus onStart() override {
    std::string file;
    if (!getInput("file", file) || file.empty()) {
      RCLCPP_ERROR(node_->get_logger(), "SetInitialPose: no route file");
      return BT::NodeStatus::FAILURE;
    }
    try {
      YAML::Node root = YAML::LoadFile(file);
      if (!root["start"]) {
        RCLCPP_ERROR(node_->get_logger(), "SetInitialPose: no 'start' in %s", file.c_str());
        return BT::NodeStatus::FAILURE;
      }
      x_ = root["start"]["x"].as<double>();
      y_ = root["start"]["y"].as<double>();
      yaw_ = root["start"]["yaw"].as<double>();
    } catch (const std::exception & e) {
      RCLCPP_ERROR(node_->get_logger(), "SetInitialPose: %s", e.what());
      return BT::NodeStatus::FAILURE;
    }
    t0_ = node_->now();
    sent_second_ = false;
    publish();
    RCLCPP_INFO(node_->get_logger(), "initial pose set to (%.2f, %.2f, yaw %.2f)", x_, y_, yaw_);
    return BT::NodeStatus::RUNNING;
  }
  BT::NodeStatus onRunning() override {
    const double t = (node_->now() - t0_).seconds();
    if (!sent_second_ && t > 1.0) { publish(); sent_second_ = true; }   // in case the first was lost
    return t > 2.5 ? BT::NodeStatus::SUCCESS : BT::NodeStatus::RUNNING;
  }
  void onHalted() override {}
private:
  void publish() {
    geometry_msgs::msg::PoseWithCovarianceStamped m;
    m.header.frame_id = "map";
    m.header.stamp = node_->now();
    m.pose.pose.position.x = x_;
    m.pose.pose.position.y = y_;
    m.pose.pose.orientation.z = std::sin(yaw_ / 2.0);
    m.pose.pose.orientation.w = std::cos(yaw_ / 2.0);
    m.pose.covariance[0] = 0.04;    // x   (0.2 m)
    m.pose.covariance[7] = 0.04;    // y
    m.pose.covariance[35] = 0.03;   // yaw (~10 deg)
    pub_->publish(m);
  }
  rclcpp::Node::SharedPtr node_;
  rclcpp::Publisher<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr pub_;
  rclcpp::Time t0_;
  double x_{0.0}, y_{0.0}, yaw_{0.0};
  bool sent_second_{false};
};

// Wait for the next region number (1..3) and turn it into the keys of its two regions in the file.
class WaitForRegion : public BT::StatefulActionNode {
public:
  WaitForRegion(const std::string & n, const BT::NodeConfig & c,
                rclcpp::Node::SharedPtr node, std::shared_ptr<Shared> s)
  : BT::StatefulActionNode(n, c), node_(node), state_(s) {}
  static BT::PortsList providedPorts() {
    return {BT::OutputPort<std::string>("load_key"), BT::OutputPort<std::string>("offload_key")};
  }
  BT::NodeStatus onStart() override {
    last_log_ = node_->now();
    if (state_->regions.empty())
      RCLCPP_INFO(node_->get_logger(),
        "Home. Waiting for the next region: ros2 topic pub --once /auracle/region std_msgs/msg/Int32 \"{data: N}\"");
    return check();
  }
  BT::NodeStatus onRunning() override { return check(); }
  void onHalted() override {}
private:
  BT::NodeStatus check() {
    while (!state_->regions.empty()) {
      const int n = state_->regions.front();
      state_->regions.pop_front();
      if (n < 1 || n > 3) {
        RCLCPP_WARN(node_->get_logger(), "region %d ignored: use 1, 2 or 3", n);
        continue;
      }
      setOutput("load_key", "load_" + std::to_string(n));
      setOutput("offload_key", "offload_" + std::to_string(n));
      RCLCPP_INFO(node_->get_logger(), "region %d: load, offload, then home", n);
      return BT::NodeStatus::SUCCESS;
    }
    if ((node_->now() - last_log_).seconds() > 30.0) {
      last_log_ = node_->now();
      RCLCPP_INFO(node_->get_logger(), "still waiting for a region number on `region` ...");
    }
    return BT::NodeStatus::RUNNING;
  }
  rclcpp::Node::SharedPtr node_;
  std::shared_ptr<Shared> state_;
  rclcpp::Time last_log_;
};

// Ask the camera which waste is on the screen and turn the answer into a region (vision_tree.xml).
// Talks to auracle_vision's waste_detector: publishes true on vision/enable (re-sent every 2 s in case
// the detector started late or a message was lost), waits for vision/decision, then publishes false
// so the detector stops using CPU. The class -> region map is in the tree XML (ports metal, organic,
// plastic), so it changes without a rebuild. A region number on the `region` topic (or route:=N)
// still wins: it is taken at once, so you can always override the camera by hand.
// timeout_s > 0: FAILURE after that long with no answer (the tree then falls back to WaitForRegion).
class WaitForDetection : public BT::StatefulActionNode {
public:
  WaitForDetection(const std::string & n, const BT::NodeConfig & c,
                   rclcpp::Node::SharedPtr node, std::shared_ptr<Shared> s)
  : BT::StatefulActionNode(n, c), node_(node), state_(s) {
    pub_ = node_->create_publisher<std_msgs::msg::Bool>("vision/enable", 10);
  }
  static BT::PortsList providedPorts() {
    return {BT::InputPort<int>("metal", 1, "region for metal"),
            BT::InputPort<int>("organic", 2, "region for organic"),
            BT::InputPort<int>("plastic", 3, "region for plastic"),
            BT::InputPort<double>("timeout_s", 0.0, "0 = wait forever"),
            BT::OutputPort<std::string>("detected_class"),
            BT::OutputPort<std::string>("load_key"),
            BT::OutputPort<std::string>("offload_key")};
  }
  BT::NodeStatus onStart() override {
    state_->vision_label.clear();      // an answer from before we asked is stale
    state_->accepting_vision = true;
    timeout_ = 0.0;
    getInput("timeout_s", timeout_);
    t0_ = node_->now();
    last_enable_ = t0_;
    enable(true);
    RCLCPP_INFO(node_->get_logger(), "Looking at the screen for the waste class ...");
    return check();
  }
  BT::NodeStatus onRunning() override { return check(); }
  void onHalted() override { stop(); }
private:
  BT::NodeStatus check() {
    // manual override: a region number on the topic beats the camera
    while (!state_->regions.empty()) {
      const int n = state_->regions.front();
      state_->regions.pop_front();
      if (n < 1 || n > 3) { RCLCPP_WARN(node_->get_logger(), "region %d ignored: use 1, 2 or 3", n); continue; }
      RCLCPP_INFO(node_->get_logger(), "region %d given by hand: camera skipped", n);
      return finish(n, "manual");
    }
    if (!state_->vision_label.empty()) {
      const std::string label = state_->vision_label;
      state_->vision_label.clear();
      int n = 0;
      if (label == "metal" || label == "organic" || label == "plastic") getInput(label, n);
      if (n >= 1 && n <= 3) {
        RCLCPP_INFO(node_->get_logger(), "camera says %s -> region %d", label.c_str(), n);
        return finish(n, label);
      }
      RCLCPP_WARN(node_->get_logger(), "camera says '%s': no region mapped to it, still looking", label.c_str());
    }
    const auto now = node_->now();
    if (timeout_ > 0.0 && (now - t0_).seconds() > timeout_) {
      RCLCPP_WARN(node_->get_logger(), "no waste class after %.0f s", timeout_);
      stop();
      return BT::NodeStatus::FAILURE;
    }
    if ((now - last_enable_).seconds() > 2.0) { last_enable_ = now; enable(true); }
    return BT::NodeStatus::RUNNING;
  }
  BT::NodeStatus finish(int n, const std::string & label) {
    stop();
    setOutput("detected_class", label);
    setOutput("load_key", "load_" + std::to_string(n));
    setOutput("offload_key", "offload_" + std::to_string(n));
    return BT::NodeStatus::SUCCESS;
  }
  void stop() { state_->accepting_vision = false; enable(false); }
  void enable(bool on) { std_msgs::msg::Bool m; m.data = on; pub_->publish(m); }
  rclcpp::Node::SharedPtr node_;
  std::shared_ptr<Shared> state_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pub_;
  rclcpp::Time t0_, last_enable_;
  double timeout_{0.0};
};

// Turn on the spot to the heading stored for `path_name` (home) in the file. Nav2 is set up to
// ignore the final heading of a goal (yaw_goal_tolerance 3.14 in nav2_params.yaml), so the tree
// does this last turn itself with Nav2's Spin behaviour. Never fails: a missed turn is only logged.
class FaceHeading : public BT::StatefulActionNode {
  using GH = rclcpp_action::ClientGoalHandle<Spin>;
public:
  FaceHeading(const std::string & n, const BT::NodeConfig & c,
              rclcpp::Node::SharedPtr node, std::shared_ptr<tf2_ros::Buffer> tf)
  : BT::StatefulActionNode(n, c), node_(node), tf_(tf) {
    client_ = rclcpp_action::create_client<Spin>(node_, "spin");
  }
  static BT::PortsList providedPorts() {
    return {BT::InputPort<std::string>("file"), BT::InputPort<std::string>("path_name")};
  }
  BT::NodeStatus onStart() override {
    std::string file, key;
    if (!getInput("file", file) || !getInput("path_name", key)) return BT::NodeStatus::SUCCESS;
    double target = 0.0;
    try {
      YAML::Node root = YAML::LoadFile(file);
      target = root[key][0]["yaw"].as<double>();
    } catch (const std::exception & e) {
      RCLCPP_WARN(node_->get_logger(), "FaceHeading: cannot read %s in %s: %s", key.c_str(), file.c_str(), e.what());
      return BT::NodeStatus::SUCCESS;
    }
    double cur = 0.0;
    try {
      auto t = tf_->lookupTransform("map", "base_footprint", tf2::TimePointZero);
      const auto & q = t.transform.rotation;
      cur = std::atan2(2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z));
    } catch (const std::exception & e) {
      RCLCPP_WARN(node_->get_logger(), "FaceHeading: no map->base_footprint transform: %s", e.what());
      return BT::NodeStatus::SUCCESS;
    }
    const double err = std::atan2(std::sin(target - cur), std::cos(target - cur));
    if (std::fabs(err) < 0.12) {
      RCLCPP_INFO(node_->get_logger(), "already facing the start heading");
      return BT::NodeStatus::SUCCESS;
    }
    if (!client_->wait_for_action_server(std::chrono::seconds(3))) {
      RCLCPP_WARN(node_->get_logger(), "spin is not available: staying at the heading it arrived with");
      return BT::NodeStatus::SUCCESS;
    }
    Spin::Goal g;
    g.target_yaw = static_cast<float>(err);
    g.time_allowance = rclcpp::Duration(20, 0);
    done_ = false; handle_.reset();
    rclcpp_action::Client<Spin>::SendGoalOptions o;
    o.goal_response_callback = [this](GH::SharedPtr h) { if (!h) done_ = true; else handle_ = h; };
    o.result_callback = [this](const GH::WrappedResult & r) {
      done_ = true;
      RCLCPP_INFO(node_->get_logger(), "turn to the start heading: %s",
                  r.code == rclcpp_action::ResultCode::SUCCEEDED ? "done" : "not completed"); };
    client_->async_send_goal(g, o);
    RCLCPP_INFO(node_->get_logger(), "turning %.0f deg to the start heading", err * 180.0 / M_PI);
    return BT::NodeStatus::RUNNING;
  }
  BT::NodeStatus onRunning() override {
    return done_ ? BT::NodeStatus::SUCCESS : BT::NodeStatus::RUNNING;
  }
  void onHalted() override { if (handle_) client_->async_cancel_goal(handle_); }
private:
  rclcpp::Node::SharedPtr node_;
  std::shared_ptr<tf2_ros::Buffer> tf_;
  rclcpp_action::Client<Spin>::SharedPtr client_;
  GH::SharedPtr handle_;
  bool done_{false};
};

// Mark mode: take six Publish Point clicks (LOAD 1..3, then OFFLOAD 1..3) and write them to the
// regions file. Each region gets the heading from its load point towards its offload point.
class RecordRegions : public BT::StatefulActionNode {
public:
  RecordRegions(const std::string & n, const BT::NodeConfig & c,
                rclcpp::Node::SharedPtr node, std::shared_ptr<Shared> s)
  : BT::StatefulActionNode(n, c), node_(node), state_(s) {}
  static BT::PortsList providedPorts() { return {BT::InputPort<std::string>("file")}; }
  BT::NodeStatus onStart() override {
    if (!getInput("file", file_) || file_.empty()) {
      RCLCPP_ERROR(node_->get_logger(), "RecordRegions: no file");
      return BT::NodeStatus::FAILURE;
    }
    pts_.clear();
    state_->clicks.clear();
    state_->recording = true;
    RCLCPP_INFO(node_->get_logger(),
      "Mark mode: RViz Publish Point tool, click the CENTRE of each region, in this order: "
      "LOAD 1, 2, 3, then OFFLOAD 1, 2, 3");
    prompt();
    return BT::NodeStatus::RUNNING;
  }
  BT::NodeStatus onRunning() override {
    auto clicks = std::move(state_->clicks);
    state_->clicks.clear();
    for (const auto & c : clicks) {
      if (!c.header.frame_id.empty() && c.header.frame_id != "map")
        RCLCPP_WARN(node_->get_logger(), "click is in frame '%s', not 'map'", c.header.frame_id.c_str());
      pts_.push_back({c.point.x, c.point.y});
      RCLCPP_INFO(node_->get_logger(), "%s = (%.2f, %.2f)", label(pts_.size() - 1).c_str(),
                  c.point.x, c.point.y);
      if (pts_.size() == 6) { state_->recording = false; return save(); }
      prompt();
    }
    return BT::NodeStatus::RUNNING;
  }
  void onHalted() override { state_->recording = false; }
private:
  static std::string label(size_t i) {
    return i < 3 ? "LOAD " + std::to_string(i + 1) : "OFFLOAD " + std::to_string(i - 2);
  }
  void prompt() {
    RCLCPP_INFO(node_->get_logger(), "click the centre of %s", label(pts_.size()).c_str());
  }
  BT::NodeStatus save() {
    std::ofstream f(file_);
    if (!f) {
      RCLCPP_ERROR(node_->get_logger(), "cannot write %s", file_.c_str());
      return BT::NodeStatus::FAILURE;
    }
    f << "# Written by mark mode (mark:=true). Map frame, metres. Heading = load point towards offload point.\n";
    f << std::fixed << std::setprecision(3);
    f << "# Where the robot is parked at launch (auto_pose:=true). (0,0,0) = where you started mapping, facing the same way.\n";
    f << "start: {x: 0.0, y: 0.0, yaw: 0.0}\n";
    f << "# Home = the start pose: the robot returns here and turns back to this heading after every region.\n";
    f << "home:\n  - {x: 0.0, y: 0.0, yaw: 0.0}\n";
    for (int n = 0; n < 3; ++n) {
      const auto & a = pts_[n];
      const auto & b = pts_[n + 3];
      const double dist = std::hypot(b[0] - a[0], b[1] - a[1]);
      if (dist < 0.5)
        RCLCPP_WARN(node_->get_logger(), "LOAD %d and OFFLOAD %d are only %.2f m apart", n + 1, n + 1, dist);
      const double yaw = std::atan2(b[1] - a[1], b[0] - a[0]);
      f << "load_" << n + 1 << ":\n  - {x: " << a[0] << ", y: " << a[1] << ", yaw: " << yaw << "}\n";
      f << "offload_" << n + 1 << ":\n  - {x: " << b[0] << ", y: " << b[1] << ", yaw: " << yaw << "}\n";
    }
    f.close();
    RCLCPP_INFO(node_->get_logger(), "saved %s. Now run without mark:=true", file_.c_str());
    return BT::NodeStatus::SUCCESS;
  }
  rclcpp::Node::SharedPtr node_;
  std::shared_ptr<Shared> state_;
  std::string file_;
  std::vector<std::array<double, 2>> pts_;
};

// Read a list of {x, y, yaw} from a yaml file (key = path_name)
class LoadWaypoints : public BT::SyncActionNode {
public:
  LoadWaypoints(const std::string & n, const BT::NodeConfig & c)
  : BT::SyncActionNode(n, c) {}
  static BT::PortsList providedPorts() {
    return {BT::InputPort<std::string>("path_name"),
            BT::InputPort<std::string>("file"),
            BT::OutputPort<Poses>("goals")};
  }
  BT::NodeStatus tick() override {
    auto log = rclcpp::get_logger("LoadWaypoints");
    std::string key, file;
    if (!getInput("path_name", key)) return BT::NodeStatus::FAILURE;
    if (!getInput("file", file) || file.empty())
      file = share_dir("auracle_bringup") + "/config/waypoints.yaml";   // old behaviour
    Poses goals;
    try {
      YAML::Node root = YAML::LoadFile(file);
      if (!root[key] || !root[key].IsSequence() || root[key].size() == 0) {
        RCLCPP_ERROR(log, "'%s' in %s is missing or empty: mark the regions first (mark:=true)",
                     key.c_str(), file.c_str());
        return BT::NodeStatus::FAILURE;
      }
      for (const auto & p : root[key]) {
        geometry_msgs::msg::PoseStamped ps;
        ps.header.frame_id = "map";
        ps.pose.position.x = p["x"].as<double>();
        ps.pose.position.y = p["y"].as<double>();
        const double yaw = p["yaw"].as<double>();
        ps.pose.orientation.z = std::sin(yaw / 2.0);
        ps.pose.orientation.w = std::cos(yaw / 2.0);
        goals.push_back(ps);
      }
    } catch (const std::exception & e) {
      RCLCPP_ERROR(log, "cannot read %s: %s", file.c_str(), e.what());
      return BT::NodeStatus::FAILURE;
    }
    RCLCPP_INFO(log, "loaded %zu points from %s", goals.size(), file.c_str());
    setOutput("goals", goals);
    return BT::NodeStatus::SUCCESS;
  }
};

// Drive the route with Nav2
class FollowWaypoints : public BT::StatefulActionNode {
  using GH = rclcpp_action::ClientGoalHandle<Nav>;
public:
  FollowWaypoints(const std::string & n, const BT::NodeConfig & c, rclcpp::Node::SharedPtr node)
  : BT::StatefulActionNode(n, c), node_(node) {
    client_ = rclcpp_action::create_client<Nav>(node_, "navigate_through_poses");
  }
  static BT::PortsList providedPorts() { return {BT::InputPort<Poses>("goals")}; }
  BT::NodeStatus onStart() override {
    Poses goals;
    if (!getInput("goals", goals) || goals.empty()) return BT::NodeStatus::FAILURE;
    if (!client_->wait_for_action_server(std::chrono::seconds(5))) {
      RCLCPP_ERROR(node_->get_logger(), "navigate_through_poses is not available");
      return BT::NodeStatus::FAILURE;
    }
    Nav::Goal g;
    g.poses = goals;
    done_ = false; ok_ = false; handle_.reset();
    rclcpp_action::Client<Nav>::SendGoalOptions o;
    o.goal_response_callback = [this](GH::SharedPtr h) {
      if (!h) { done_ = true; ok_ = false; } else { handle_ = h; } };
    o.result_callback = [this](const GH::WrappedResult & r) {
      done_ = true;
      ok_ = (r.code == rclcpp_action::ResultCode::SUCCEEDED);
      RCLCPP_INFO(node_->get_logger(), "route finished: %s", ok_ ? "succeeded" : "failed"); };
    client_->async_send_goal(g, o);
    RCLCPP_INFO(node_->get_logger(), "driving %zu points", goals.size());
    return BT::NodeStatus::RUNNING;
  }
  BT::NodeStatus onRunning() override {
    if (!done_) return BT::NodeStatus::RUNNING;
    return ok_ ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
  }
  void onHalted() override { if (handle_) client_->async_cancel_goal(handle_); }
private:
  rclcpp::Node::SharedPtr node_;
  rclcpp_action::Client<Nav>::SharedPtr client_;
  GH::SharedPtr handle_;
  bool done_{false}, ok_{false};
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("bt_runner");   // use_sim_time comes from the launch file
  node->declare_parameter<std::string>("tree_file", "");
  node->declare_parameter<std::string>("route_file", "");
  node->declare_parameter<int>("route", 0);   // first region to run (1..3); 0 = wait for one on topic `region`
  node->declare_parameter<bool>("mark", false);   // true: record the regions instead of driving
  node->declare_parameter<bool>("auto_pose", true);   // true: set the start pose from regions.yaml, no RViz click
  node->declare_parameter<bool>("vision", false);   // true: the region comes from the camera (vision_tree.xml)

  std::string tree_file = node->get_parameter("tree_file").as_string();
  std::string route_file = node->get_parameter("route_file").as_string();
  const std::string share = share_dir("auracle_bt");
  const bool mark = node->get_parameter("mark").as_bool();
  const bool auto_pose = node->get_parameter("auto_pose").as_bool();
  if (tree_file.empty()) {
    if (mark) tree_file = "mark_tree.xml";
    else if (node->get_parameter("vision").as_bool())
      tree_file = auto_pose ? "vision_tree.xml" : "vision_click_tree.xml";
    else tree_file = auto_pose ? "region_tree.xml" : "region_click_tree.xml";
  }
  if (tree_file.find('/') == std::string::npos) tree_file = share + "/behavior_trees/" + tree_file;
  if (route_file.empty()) route_file = share + "/config/regions.yaml";

  auto state = std::make_shared<Shared>();
  auto label_sub = node->create_subscription<std_msgs::msg::String>(
    "/detected_class", 10,
    [state](std_msgs::msg::String::SharedPtr m) { state->label = m->data; });
  // AMCL latches its last pose, so a late subscriber still gets it (transient local).
  auto amcl_sub = node->create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
    "amcl_pose", rclcpp::QoS(1).transient_local().reliable(),
    [state](geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr) { state->have_amcl = true; });

  // Region numbers to run, one at a time (1..3). route:=N puts the first one in the queue.
  auto region_sub = node->create_subscription<std_msgs::msg::Int32>(
    "region", 10,
    [state](std_msgs::msg::Int32::SharedPtr m) { state->regions.push_back(m->data); });
  {
    const int first = static_cast<int>(node->get_parameter("route").as_int());
    if (first > 0) state->regions.push_back(first);
  }
  // map -> base_footprint, for the final turn at home (launch remaps /tf to tf in the namespace)
  auto tf_buffer = std::make_shared<tf2_ros::Buffer>(node->get_clock());
  auto tf_listener = std::make_shared<tf2_ros::TransformListener>(*tf_buffer, node, false);

  // RViz "Publish Point" (mark mode). Kept only while RecordRegions runs.
  auto click_sub = node->create_subscription<geometry_msgs::msg::PointStamped>(
    "clicked_point", 10,
    [state](geometry_msgs::msg::PointStamped::SharedPtr m) {
      if (state->recording) state->clicks.push_back(*m); });

  // RViz "2D Pose Estimate" publishes here (AMCL reads the same topic). Volatile: only seen while running.
  auto pose_sub = node->create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
    "initialpose", 10,
    [state](geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr) { state->have_pose_estimate = true; });
  // RViz "2D Goal Pose" (launched with goal_topic:=mission_goal). Taken only while WaitForGoal runs.
  auto goal_sub = node->create_subscription<geometry_msgs::msg::PoseStamped>(
    "mission_goal", 10,
    [state, node](geometry_msgs::msg::PoseStamped::SharedPtr m) {
      if (!state->accepting_goal) {
        RCLCPP_WARN(node->get_logger(), "goal ignored: not ready for one (set the pose estimate first, or the robot is still driving)");
        return;
      }
      state->goal = *m;
      state->goal_ready = true; });

  // Camera answer (auracle_vision waste_detector). Taken only while WaitForDetection runs.
  auto vision_sub = node->create_subscription<std_msgs::msg::String>(
    "vision/decision", 10,
    [state](std_msgs::msg::String::SharedPtr m) {
      if (state->accepting_vision) state->vision_label = m->data; });

  BT::BehaviorTreeFactory factory;
  factory.registerNodeType<GetDetectedClass>("GetDetectedClass", state);
  factory.registerNodeType<WaitForNav2>("WaitForNav2", node, state);
  factory.registerNodeType<WaitForInitialPose>("WaitForInitialPose", node, state);
  factory.registerNodeType<WaitForGoal>("WaitForGoal", node, state);
  factory.registerNodeType<NavigateToGoal>("NavigateToGoal", node);
  factory.registerNodeType<SetInitialPose>("SetInitialPose", node);
  factory.registerNodeType<WaitForRegion>("WaitForRegion", node, state);
  factory.registerNodeType<WaitForDetection>("WaitForDetection", node, state);
  factory.registerNodeType<FaceHeading>("FaceHeading", node, tf_buffer);
  factory.registerNodeType<RecordRegions>("RecordRegions", node, state);
  factory.registerNodeType<LoadWaypoints>("LoadWaypoints");
  factory.registerNodeType<FollowWaypoints>("FollowWaypoints", node);

  auto tree = factory.createTreeFromFile(tree_file);
  tree.rootBlackboard()->set("route_file", route_file);   // the trees read it as {route_file}
  RCLCPP_INFO(node->get_logger(), "tree: %s | file: %s", tree_file.c_str(), route_file.c_str());

  rclcpp::Rate rate(10);
  BT::NodeStatus status = BT::NodeStatus::RUNNING;
  while (rclcpp::ok() && status == BT::NodeStatus::RUNNING) {
    rclcpp::spin_some(node);
    status = tree.tickOnce();
    rate.sleep();
  }
  tree.haltTree();   // cancels the Nav2 goal if we were interrupted
  RCLCPP_INFO(node->get_logger(), "Tree finished: %s", BT::toStr(status).c_str());
  rclcpp::shutdown();
  return status == BT::NodeStatus::SUCCESS ? 0 : 1;
}