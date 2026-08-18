import rclpy
from rclpy.node import Node
from std_msgs.msg import String

class PublisherNode(Node):
    def __init__(self):
        super().__init__('publisher_node')
        self.pub = self.create_publisher(String, 'chatter', 10)
        self.timer = self.create_timer(1.0, self.publish_msg)

    def publish_msg(self):
        msg = String()
        msg.data = "Hello from publisher"
        self.pub.publish(msg)
        self.get_logger().info("Publishing: %s" % msg.data)

def main():
    rclpy.init()
    node = PublisherNode()
    rclpy.spin(node)
    rclpy.shutdown()

