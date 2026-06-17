#! /usr/bin/env python3
""" 
Description: 
    This ros2 node periodically publishes 'kit rules' messages to a topic.
-----
Publishing topics: 
    The channel containing the 'kit rules' messages
    /my_topic1 - std_msgs/String
-----
Subscription topics:
    none
-----
Author: kitana
Date : 9/3/2026
"""

import rclpy #imports ros2 client library for py
from rclpy.node import Node #imports node class for creating nodes
from std_msgs.msg import String #imports string messgae type for ros2

class Minpypub(Node):
    #creates a minimal publisher node
    def __init__(self):
        ##creates custom node class for publishing messages
        #initializes the node with a name
        super().__init__('minpypub') 
        #create a publisher object for the topic, [message type, topic name, queue size]
        self.publisher_1= self.create_publisher(String, '/my_topic1', 10) 
        #timer with period of 0.5 seconds to triggger messae publishing
        timer_period_1 = 0.5
        self.timer_1= self.create_timer(timer_period_1, self.timer_callback)
        #counter variable for message content(label for message)
        self.msg_label = 0

    def timer_callback(self):
        #function executed periodically by the timer, creates blank string message my_msg
        my_msg = String()
        #sets message with counter
        my_msg.data = "kit rules: %d" %self.msg_label
        #publishes message to the topic
        self.publisher_1.publish(my_msg)
        #log message indicating message has been published
        self.get_logger().info("Publishing; %s" %my_msg.data)
        self.msg_label += 1

def main(args=None):
    #main fxn to start the ros2 node
    rclpy.init(args=args)
    #create instance of minimal publisher node
    myminpy = Minpypub()
    rclpy.spin(myminpy)
    #destroy node explicitly
    myminpy.destroy_node()
    #shutdown ros2 comms
    rclpy.shutdown()
    
if __name__ == '__main__':
    #execute main function if run directly
    main()