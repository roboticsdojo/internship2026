#!/usr/bin/env python3
"""
Description: This ROS 2 node acts as a safety monitor for the rover's power system.
It listens to the live battery voltage broadcasts and triggers a critical warning
if the power drops below the safe threshold required to return to the launch base.
"""

import rclpy
from rclpy.node import Node
from std_msgs.msg import Float32

class BatterySubscriber(Node):
    def __init__(self):
        # 1. Initialize the node and register it with the ROS 2 network
        super().__init__('battery_subscriber')
        
        # 2. Create the listening ear (Subscription)
        self.subscription = self.create_subscription(
            Float32,
            'rover/battery_voltage',
            self.voltage_callback,
            10
        )

    def voltage_callback(self, msg):
        # Extract the raw decimal number from the message payload
        voltage = msg.data
        
        # 3. The Decision Logic: Check if the voltage has crossed our safety threshold
        if voltage < 11.5:
            # Use a 'warning' logger to print yellow text in the terminal
            self.get_logger().warning(f'CRITICAL: Voltage dropped to {voltage:.2f}V! Initiate return to base.')
        else:
            # Use a standard 'info' logger for normal operational telemetry
            self.get_logger().info(f'Voltage stable at: {voltage:.2f}V')

def main(args=None):
    rclpy.init(args=args)
    node = BatterySubscriber()
    
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()