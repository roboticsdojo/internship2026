#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <chrono>

using namespace std::chrono_literals;

class MyNode : public rclcpp::Node
{
public:
    MyNode() : Node("my_cpp_node")
    {
        // --- PUBLISHER ---
        publisher_ = this->create_publisher<std_msgs::msg::String>("/my_topic", 10);
        timer_ = this->create_wall_timer(1s, std::bind(&MyNode::publish_message, this));

        // --- SUBSCRIBER ---
        subscriber_ = this->create_subscription<std_msgs::msg::String>(
            "/my_topic",
            10,
            std::bind(&MyNode::subscriber_callback, this, std::placeholders::_1)
        );

        RCLCPP_INFO(this->get_logger(), "Node has started!");
    }

private:
    // --- PUBLISHER CALLBACK ---
    void publish_message()
    {
        auto msg = std_msgs::msg::String();
        msg.data = "Hello from my_cpp_node!";
        publisher_->publish(msg);
        RCLCPP_INFO(this->get_logger(), "Published: '%s'", msg.data.c_str());
    }

    // --- SUBSCRIBER CALLBACK ---
    void subscriber_callback(const std_msgs::msg::String::SharedPtr msg)
    {
        RCLCPP_INFO(this->get_logger(), "Received: '%s'", msg->data.c_str());
    }

    // --- MEMBER VARIABLES ---
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscriber_;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<MyNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}