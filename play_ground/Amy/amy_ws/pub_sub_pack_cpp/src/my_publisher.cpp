//Copyright 2026 Amy Kibara
/*
 * @author Amy Kibara
 * @file my_publisher.cpp
 * @brief This ros2 node periodically publishes messages to a topic
 * @version 0.1
 * @date 2026.06.17
 * ------
 * Subscription topics:
 *  None
 * -----
 * Publishing topics:
 *  String message
 *  /greetings_cpp - std_msgs/String
*/


#include "rclcpp/rclcpp.hpp" //ros2 c++ client library
#include "std_msgs/msg/string.hpp" //std message type for strings

using namespace std::chrono_literals; // handles time literals

class Mincpppub : public rclcpp::Node
{
public:
  Mincpppub()
  : Node("min_cpp_pub"), count_(0)
  {
    publisher_1 = create_publisher<std_msgs::msg::String>("/greetings_cpp", 10);
        //Timer with period of 0.5s, callback fxn is timerCallback, binds callback to publisher
    timer_ = create_wall_timer(500ms, std::bind(&Mincpppub::timerCallback, this));
  }
  void timerCallback()
  {
    auto message = std_msgs::msg::String();
    message.data = "Hello there " + std::to_string(count_++);
    publisher_1->publish(message);
    RCLCPP_INFO_STREAM(get_logger(), "Publishing: " << message.data);
  }

private:
  size_t count_;  //keeps track of the number of messages published
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_1;   // publisher object
  rclcpp::TimerBase::SharedPtr timer_;
};

#ifndef TESTING_EXCLUDE_MAIN
int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto mincppnode = std::make_shared<Mincpppub>();
  rclcpp::spin(mincppnode);
  rclcpp::shutdown();
  return 0;
}
#endif
