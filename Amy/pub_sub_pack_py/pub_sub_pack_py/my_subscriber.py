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
    This ROS2 node subscribes to 'Hello there!' messages
-----
Publishing topics
    None
-----
Subscription topics
    The channel containing the 'Hello there!' messages
    /greetings_py - std_msgs/String
-----
Author: Amy Kibara
Date : 17/6/2026
"""

import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class Minpysub(Node):
    def __init__(self):
        super().__init__('minpysub')
        # [msg_type, topic name, callback fxn, queue size]
        self.subscriber_1 = self.create_subscription(
            String, '/greetings_py', self.listener_callback, 10)

    def listener_callback(self, msg):
        # prints out every time it receives a message
        self.get_logger().info(f'I heard "{msg.data}"')


def main(args=None):
    rclpy.init(args=args)  # initializes ros2 comms
    myminsub = Minpysub()
    rclpy.spin(myminsub)  # keeps node running
    myminsub.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()  # executes main if script run directly