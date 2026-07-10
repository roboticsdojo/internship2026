# Copyright 2026 Amy Kibara
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


#!/usr/bin/env python3
"""
Description:
    This ros2 node periodically publishes 'Hello there!' messages to a topic.
-----
Publishing topics:
    The channel containing the 'Hello there!' messages
    /greetings_py - std_msgs/String
-----
Subscription topics:
    none
-----
Author: Amy Kibara
Date : 17/6/2026
"""

import rclpy  # imports ros2 client library for py
from rclpy.node import Node  # imports node class for creating nodes
from std_msgs.msg import String  # imports string messgae type for ros2


class Minpypub(Node):
    # creates a minimal publisher node
    def __init__(self):
        super().__init__('minpypub')
        # create a publisher object for the topic, [message type, topic name,
        # queue size]
        self.publisher_1 = self.create_publisher(String, '/greetings_py', 10)
        # timer with period of 0.5 seconds to triggger message publishing
        timer_period_1 = 0.5
        self.timer_1 = self.create_timer(timer_period_1, self.timer_callback)
        # counter variable for message content(label for message)
        self.msg_label = 0

    def timer_callback(self):
        # function executed periodically by the timer, creates blank string
        # message my_msg
        my_msg = String()
        # sets message with counter
        my_msg.data = "Hello there!: %d" % self.msg_label
        # publishes message to the topic
        self.publisher_1.publish(my_msg)
        # log message indicating message has been published
        self.get_logger().info("Publishing; %s" % my_msg.data)
        self.msg_label += 1


def main(args=None):
    # main fxn to start the ros2 node
    rclpy.init(args=args)
    # create instance of minimal publisher node
    myminpy = Minpypub()
    rclpy.spin(myminpy)
    # destroy node explicitly
    myminpy.destroy_node()
    # shutdown ros2 comms
    rclpy.shutdown()


if __name__ == '__main__':
    # execute main function if run directly
    main()
