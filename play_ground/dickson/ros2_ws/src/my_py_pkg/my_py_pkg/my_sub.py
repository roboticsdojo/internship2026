import rclpy
from rclpy.node import Node

from std_msgs.msg import String


class MinimalSubscriber(Node):

    def __init__(self):
        super().__init__('minimal_subscriber')
#he constructor creates a subscriber with the same arguments as the publisher using create_subscription. 
        self.subscription = self.create_subscription(
            String,
            'topic',
            self.listener_callback,
            10)

    def listener_callback(self, msg):
        self.get_logger().info(f'I heard: "{msg.data}"')

#First the rclpy library is initialized, then the node is created, and then it “spins” the node (using spin()) so its callbacks are called
def main(args=None):
    rclpy.init(args=args)

    node = MinimalSubscriber()

    rclpy.spin(node)

    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()