#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

#include "behaviortree_cpp/bt_factory.h"
#include "behaviortree_cpp/loggers/groot2_publisher.h"
#include "behaviortree_cpp/xml_parsing.h"
#include "bt_nodes.hpp"
#include "rclcpp/rclcpp.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("mission_bt");

  const auto tree_file = node->declare_parameter<std::string>("tree_file", "");
  const auto groot_port = node->declare_parameter<int>("groot_port", 1667);
  const auto model_file = node->declare_parameter<std::string>("model_file", "");

  BT::BehaviorTreeFactory factory;
  factory.registerNodeType<robot_2_behavior::NavigateToPose>("NavigateToPose", node);
  factory.registerNodeType<robot_2_behavior::SpeedAdapter>("SpeedAdapter", node);

  // Optional: dump the node model so Groot2's editor knows our custom nodes.
  if (!model_file.empty()) {
    std::ofstream(model_file) << BT::writeTreeNodesModelXML(factory);
  }

  factory.registerBehaviorTreeFromFile(tree_file);
  auto tree = factory.createTree("MainTree");

  // Groot2 (Monitor tab) connects to <robot-ip>:groot_port
  BT::Groot2Publisher groot(tree, static_cast<unsigned>(groot_port));

  RCLCPP_INFO(node->get_logger(), "Mission started (Groot2 on port %d)", groot_port);

  BT::NodeStatus status = BT::NodeStatus::RUNNING;
  while (rclcpp::ok() && status == BT::NodeStatus::RUNNING) {
    rclcpp::spin_some(node);
    status = tree.tickOnce();
    tree.sleep(std::chrono::milliseconds(50));
  }
  RCLCPP_INFO(node->get_logger(), "Mission finished: %s", BT::toStr(status).c_str());

  // Stay alive so Groot2 can still show the final state.
  while (rclcpp::ok()) {
    rclcpp::spin_some(node);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  rclcpp::shutdown();
  return 0;
}