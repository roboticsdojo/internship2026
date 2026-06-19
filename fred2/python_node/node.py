import rclpy
from rclpy.node import Node
from std_msgs.msg import String

class MyNode(Node):

    def __init__(self):
        super().__init__('my_node')  # node name

        # --- PUBLISHER ---
        # publishes to topic '/my_topic' every 1 second
        self.publisher_ = self.create_publisher(String, '/my_topic', 10)
        self.timer = self.create_timer(1.0, self.publish_message)

        # --- SUBSCRIBER ---
        # listens to topic '/my_topic'
        self.subscriber_ = self.create_subscription(
            String,
            '/my_topic',
            self.subscriber_callback,
            10
        )

        self.get_logger().info('Node has started!')

    def publish_message(self):
        msg = String()
        msg.data = 'Hello from my_node!'
        self.publisher_.publish(msg)
        self.get_logger().info(f'Published: "{msg.data}"')

    def subscriber_callback(self, msg):
        self.get_logger().info(f'Received: "{msg.data}"')


def main(args=None):
    rclpy.init(args=args)
    node = MyNode()
    rclpy.spin(node)       # keeps the node running
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()