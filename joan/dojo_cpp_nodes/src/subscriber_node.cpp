#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

class DojoSubscriber : public rclcpp::Node {
public:
    DojoSubscriber() : Node("cpp_subscriber_node") {
        
        // std::bind is how C++ points to the callback function when a message arrives
        subscription_ = this->create_subscription<std_msgs::msg::String>(
            "dojo_comms", 10, std::bind(&DojoSubscriber::topic_callback, this, std::placeholders::_1));
    }

private:
    void topic_callback(const std_msgs::msg::String::SharedPtr msg) const {
        RCLCPP_INFO(this->get_logger(), "I heard: '%s'", msg->data.c_str());
    }

    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<DojoSubscriber>());
    rclcpp::shutdown();
    return 0;
}