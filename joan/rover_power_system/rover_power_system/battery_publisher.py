#!/usr/bin/env python3    #tells ubuntu that this file should be executed using python3......
# Simulating the Vulcan rover's battery telemetry so I can test the safety triggers.

import rclpy
from rclpy.node import Node
from std_msgs.msg import Float32

class BatteryPublisher(Node):
    def __init__(self):
        # Registering the node so the rest of the rover stack can see it
        super().__init__('battery_publisher')
        
        # Setting up the publisher to broadcast on the battery channel.
        # Keeping the queue size at 10 to avoid dropping readings if the network lags.
        self.publisher_ = self.create_publisher(Float32, 'rover/battery_voltage', 10)
        
        # Ticking every 1 second (1Hz should be plenty fast for battery monitoring)
        self.timer = self.create_timer(1.0, self.publish_voltage)
        
        # Starting with a fully charged 3S LiPo battery profile
        self.current_voltage = 12.6 

    def publish_voltage(self):
        # Create a fresh message box for the decimal value
        msg = Float32()
        msg.data = self.current_voltage
        
        # Push the reading out to the ROS 2 network
        self.publisher_.publish(msg)
        
        # Log it to the terminal so I can verify it is actually running
        self.get_logger().info(f'Broadcasting voltage: {msg.data:.2f}V')
        
        # Simulate the drive motors draining the battery by dropping 0.05V per tick
        self.current_voltage -= 0.05 

def main(args=None):
    # Boot up the ROS 2 engine
    rclpy.init(args=args)
    node = BatteryPublisher()
    
    try:
        # Trap it in a loop so it keeps ticking and broadcasting
        rclpy.spin(node)
    except KeyboardInterrupt:
        # Catching Ctrl+C so it shuts down quietly without throwing massive Python tracebacks
        pass
    finally:
        # Clean up the node before exiting
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()