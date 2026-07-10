#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

class FirstSubscriber : public rclcpp::Node
{
    // The FirstSubscriber class inherits from the rclcpp::Node class, which is the base class for all ROS 2 nodes. 
    //It represents a subscriber node that listens to messages on the topic 'first_topic' and processes them using a callback function.
    public: 
     FirstSubscriber() : Node("first_subscriber")
     { 
        auto topic_callback = [this] (std_msgs::msg::String::UniquePtr msg) -> void {
            RCLCPP_INFO(this ->get_logger(), "I heard: '%s'", msg->data.c_str());
        };
        subscription_ = this-> create_subscription<std_msgs::msg::String>("first_topic", 10, topic_callback);
     } //create_subscription executes the callback function when a message is received on the topic

     //topic_callback is the function that receives string message data over the topic and writes it to the console

    private:
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
};

// The main function initializes the ROS 2 system, creates an instance of the FirstSubscriber node, and starts spinning to process incoming messages. It will continue to run until the node is shut down, at which point it will clean up and exit.
int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    rclcpp:: spin(std::make_shared<FirstSubscriber>());
    rclcpp::shutdown();
    return 0;
}