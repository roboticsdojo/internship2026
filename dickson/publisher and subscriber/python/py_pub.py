#imports rclpy s.o its node can be used
import rclpy
from rclpy.node import Node
#importd the build in message type that the node uses to structue the data that it passes on the topic
from std_msgs.msg import String


class MinimalPublisher(Node):

    def __init__(self):
        super()__init__('minimal_publisher')
        #declares that the node publishes messages of type std_msgs/msg/String (imported from the std_msgs.msg module), over a topic named topic, and that the “queue size” is 10.
        self.publisher_ = self.create_publisher(String, 'topic', 10)
        #used to create a callback that executes every 0.5 seconds
        self.timer = self.create_timer(1.0, self.timer_callback)
        self.i = 0
#reates a message with the counter value appended, publishes it, and prints it to the console with get_logger()’s info() function.
    def timer_callback(self):
        msg = String()
        msg.data = f'Hello {self.i}'
        self.publisher_.publish(msg)
        self.get_logger().info(f'Publishing: "{msg.data}"')
        self.i += 1 #is a counter used in the callback.

#First the rclpy library is initialized, then the node is created, and then it “spins” the node (using spin()) so its callbacks are called
def main(args=None):
    rclpy.init(args=args)

    node = MinimalPublisher()

    rclpy.spin(node)

    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()