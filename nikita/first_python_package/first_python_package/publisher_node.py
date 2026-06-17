import rclpy    #import the rclpy library which contains tools to build the python node
from rclpy.node import Node #import the Node class from the rclpy library 

from std_msgs.msg import String  #imports the built-in std_msgs/msg/String message type that the node uses to structure the data that it passes on the topic.

class FirstPublisherNode(Node): #the class FirstPublisherNode inherits from Node 
    def __init__(self):
        super().__init__('first_publisher_node') #constructor
        self.publisher_ = self.create_publisher(String, 'first_topic', 10) #messages will be published as String messages on the topic first_topic with a message queue of size 10
        timer_period= 0.5 #the timer callback function will be called every 0.5 seconds
        self.timer = self.create_timer(timer_period, self.timer_callback)
        self.i=0
        
        # the timer callback function is called every 0.5 seconds, and it creates a new String message, sets its data field to a string that includes the current value of the counter i, publishes the message on the topic first_topic, logs the published message to the console, and increments the counter i by 1.
    def timer_callback(self):
        msg= String()
        msg.data = 'Hello World: %d' % self.i
        self.publisher_.publish(msg)
        self.get_logger().info('Publishing: "%s"' %msg.data)
        self.i +=1

        # the main function initializes the ROS 2 client library, creates an instance of the FirstPublisherNode class, and starts spinning the node to process incoming messages and execute the timer callback function. When the node is shut down, it destroys the node and shuts down the ROS 2 client library.
    def main(args=None):
        rclpy.init(args=args)
        first_publisher_node= FirstPublisherNode()
        rclpy.spin(first_publisher_node)
        first_publisher_node.destroy_node()
        rclpy.shutdown()

    



