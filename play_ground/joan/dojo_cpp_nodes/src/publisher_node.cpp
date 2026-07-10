// 1. INCLUDES (The C++ version of 'import')
#include <chrono>
#include <memory>
#include "rclcpp/rclcpp.hpp"        // The ROS 2 C++ engine
#include "std_msgs/msg/string.hpp"  // The standard text message type

using namespace std::chrono_literals;

// 2. CLASS DEFINITION
// We inherit from rclcpp::Node, just like 'class MyNode(Node):' in Python
class DojoPublisher : public rclcpp::Node {
public:
    // This is the Constructor (the exact equivalent of __init__(self) in Python)
    DojoPublisher() : Node("cpp_publisher_node"), count_(0) {
        
        // Create the Publisher (Message Type, Topic Name, Queue Size)
        publisher_ = this->create_publisher<std_msgs::msg::String>("dojo_comms", 10);
        
        // Create the Timer (Ticks every 1000 milliseconds / 1 second)
        timer_ = this->create_wall_timer(
            1000ms, std::bind(&DojoPublisher::timer_callback, this));
    }

private:
    // The Callback function that runs every time the timer ticks
    void timer_callback() {
        auto message = std_msgs::msg::String();
        message.data = "Dojo C++ Node Active! Broadcast count: " + std::to_string(count_++);
        
        // RCLCPP_INFO is the C++ equivalent of self.get_logger().info()
        RCLCPP_INFO(this->get_logger(), "Publishing: '%s'", message.data.c_str());
        
        // Broadcast it to the network
        publisher_->publish(message);
    }

    // 3. VARIABLE DECLARATIONS
    // In C++, you MUST declare your variables at the bottom of the class
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
    size_t count_;
};

// 4. THE MAIN EXECUTION LOOP
int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<DojoPublisher>()); // Keeps the node alive
    rclcpp::shutdown();
    return 0;
}