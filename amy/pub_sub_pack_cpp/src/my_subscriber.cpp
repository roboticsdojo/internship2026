/* 
 * @file cppminsub.cpp
 * @author Kitana
 * @brief Demonstrates cpp subscribing
 * @version 0.1
 * @date 2026.03.13
 * ------
 * Subscription topics:
 *  String message
 *  /my_topic2 - std_msgs/String
 * -----
 * Publishing topics:
 *  None-its just a subscriber
*/

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using std::placeholders::_1; //placeholder for callback fxn

class Mincppsub : public rclcpp::Node
{
    public://stuff within node() is name of node
        Mincppsub(): Node("min_cpp_sub")
        {
        //create var to store the sub, give it message type, topic name, queue size, fxn that's bound to it and will be executed every time a message is published on topic
            subscriber_1 = 
            create_subscription<std_msgs::msg::String>("/my_topic2", 10, std::bind(&Mincppsub::topicCallback, this, _1));
        }
        //callback fxn
        void topicCallback(const std_msgs::msg::String & msg) const
        {
            //every time message received, prints it out
            RCLCPP_INFO_STREAM(get_logger(), "I heard : " << msg.data);
        }
    private:
        //member vars
        rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscriber_1;
};

//initialize system, create node instance, shutdown ros2 when program end
int main(int argc, char * argv[]){
    rclcpp::init(argc, argv);
    auto min_cpp_sub_node = std::make_shared<Mincppsub>();
    rclcpp::spin(min_cpp_sub_node);
    rclcpp::shutdown();
    return 0;
}