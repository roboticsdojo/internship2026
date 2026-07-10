#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include <chrono>
#include <functional>
#include <memory>
using namespace std::chrono_literals;

class MyFirstNode : public rclcpp::Node {
public:
    MyFirstNode() : Node("cpp_subscriber") {
        subscription_ = this->create_subscription<std_msgs::msg::String>(
            "hello_topic", 10,
            std::bind(&MyFirstNode::topic_callback, this, std::placeholders::_1));
    }

private:
    void topic_callback(const std_msgs::msg::String::SharedPtr msg) {
        RCLCPP_INFO(this->get_logger(), "I heard: '%s'", msg->data.c_str());
    }
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<MyFirstNode>());
    rclcpp::shutdown();
    return 0;
}