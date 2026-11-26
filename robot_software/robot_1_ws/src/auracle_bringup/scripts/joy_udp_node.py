#!/usr/bin/env python3
"""
WSL side: receive UDP packets from joy_bridge_win.py and publish sensor_msgs/Joy.

    ros2 run auracle_bringup joy_udp_node.py                              # sim: /joy
    ros2 run auracle_bringup joy_udp_node.py --ros-args -r __ns:=/auracle  # robot: /auracle/joy
    ros2 run auracle_bringup joy_udp_node.py --ros-args -p port:=5005 -p timeout:=0.25

Safety: if packets stop arriving for `timeout` seconds (bridge killed, Windows
asleep, UDP path broken), one neutral Joy (all buttons released) is published so
teleop_twist_joy sees "enable button released" and sends a zero command.
"""
import json
import socket
import time

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Joy

NEUTRAL_AXES = [0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0]
NEUTRAL_BUTTONS = [0] * 11


class JoyUdpNode(Node):
    def __init__(self):
        super().__init__("joy_udp_node")
        self.declare_parameter("port", 5005)
        self.declare_parameter("timeout", 0.25)
        port = self.get_parameter("port").value
        self.timeout = self.get_parameter("timeout").value

        self.pub = self.create_publisher(Joy, "joy", 10)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("0.0.0.0", port))
        self.sock.setblocking(False)

        self.last_rx = None          # monotonic time of the last packet
        self.neutral_sent = True     # nothing to cancel until a packet arrives
        self.create_timer(0.005, self.poll)  # 200 Hz drain loop
        self.get_logger().info(f"Listening for controller packets on UDP {port}")

    def publish(self, axes, buttons):
        msg = Joy()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.header.frame_id = "joy"
        msg.axes = [float(a) for a in axes]
        msg.buttons = [int(b) for b in buttons]
        self.pub.publish(msg)

    def poll(self):
        latest = None
        while True:  # drain the socket, publish only the newest packet
            try:
                data, _ = self.sock.recvfrom(4096)
            except BlockingIOError:
                break
            try:
                latest = json.loads(data)
            except json.JSONDecodeError:
                self.get_logger().warn("Bad packet ignored")

        now = time.monotonic()
        if latest is not None:
            if self.last_rx is None or self.neutral_sent:
                self.get_logger().info("Controller packets flowing")
            self.last_rx = now
            self.neutral_sent = False
            self.publish(latest["axes"], latest["buttons"])
        elif (not self.neutral_sent and self.last_rx is not None
              and now - self.last_rx > self.timeout):
            self.get_logger().warn(
                f"No controller packets for {self.timeout}s: publishing neutral")
            self.publish(NEUTRAL_AXES, NEUTRAL_BUTTONS)
            self.neutral_sent = True


def main():
    rclpy.init()
    node = JoyUdpNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.try_shutdown()


if __name__ == "__main__":
    main()