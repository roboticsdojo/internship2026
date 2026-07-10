#!/usr/bin/env python#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from std_msgs.msg import String

class MyFirstNode(Node):
    def __init__(self):
        super().__init__('python_publisher')
        self.publisher_ = self.create_publisher(String, 'hello_topic', 10)
        self.create_timer(1.0, self.timer_callback)

    def timer_callback(self):
        msg = String()
        msg.data = 'Hello dojo2026!'
        self.publisher_.publish(msg)
        self.get_logger().info(f'Publishing: {msg.data}')

def main(args=None): 
    rclpy.init(args=args)
    rclpy.spin(MyFirstNode())
    rclpy.shutdown()

if __name__ == '__main__':
    main()