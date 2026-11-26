#!/usr/bin/env python3
"""odom_cov_relay: give rf2o's laser odometry a real covariance so the EKF can weigh it.

rf2o_laser_odometry publishes nav_msgs/Odometry with all-zero covariances. robot_localization
treats that as "perfectly certain", so a bad scan match (long plain wall, lidar lockup) would
be believed over everything else. This node copies the message and fills the diagonal.

  in : odom_rf2o_raw   (nav_msgs/Odometry, from rf2o)
  out: odom_rf2o       (same, with covariance)  -> fused by ekf_lidar_odom.yaml as odom1

If the scan matcher stops publishing, nothing is relayed and the EKF simply coasts on the IMU.

Messages with a NaN / inf anywhere, or a speed the robot cannot reach, are dropped here. One such
message is enough to put NaN into robot_localization's state for good ("Critical Error, NaNs were
detected in the output state of the filter"), and rf2o can produce one on its first scans
(velocity = distance / dt with a dt of ~0).
"""
import math

import rclpy
from nav_msgs.msg import Odometry
from rclpy.node import Node


class OdomCovRelay(Node):
    def __init__(self):
        super().__init__('odom_cov_relay')
        # Variance on the diagonal of the 6x6 (x, y, z, roll, pitch, yaw) twist / pose blocks.
        self.declare_parameter('twist_vx_var', 0.01)    # (0.1 m/s)^2: rf2o is good to ~cm/s
        self.declare_parameter('twist_vyaw_var', 0.02)
        self.declare_parameter('pose_var', 0.05)        # pose is not fused; kept for RViz
        self.vx = float(self.get_parameter('twist_vx_var').value)
        self.vyaw = float(self.get_parameter('twist_vyaw_var').value)
        self.pv = float(self.get_parameter('pose_var').value)
        # The robot tops out at 0.25 m/s and turns at ~1 rad/s: anything far above is a bad match.
        self.declare_parameter('max_speed', 1.0)        # m/s
        self.declare_parameter('max_yaw_rate', 6.0)     # rad/s
        self.max_v = float(self.get_parameter('max_speed').value)
        self.max_w = float(self.get_parameter('max_yaw_rate').value)
        self.dropped = 0
        self.pub = self.create_publisher(Odometry, 'odom_rf2o', 10)
        self.create_subscription(Odometry, 'odom_rf2o_raw', self.on_odom, 10)

    @staticmethod
    def _finite(m: Odometry) -> bool:
        p = m.pose.pose
        t = m.twist.twist
        vals = (p.position.x, p.position.y, p.position.z,
                p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w,
                t.linear.x, t.linear.y, t.linear.z, t.angular.x, t.angular.y, t.angular.z)
        return all(math.isfinite(v) for v in vals)

    def on_odom(self, m: Odometry):
        t = m.twist.twist
        if not self._finite(m) or abs(t.linear.x) > self.max_v or abs(t.angular.z) > self.max_w:
            self.dropped += 1
            self.get_logger().warn(
                f'dropped a bad rf2o message (NaN or implausible speed), {self.dropped} so far',
                throttle_duration_sec=5.0)
            return
        pose = [0.0] * 36
        twist = [0.0] * 36
        for i in range(6):
            pose[i * 6 + i] = self.pv if i in (0, 1, 5) else 1e6
            twist[i * 6 + i] = 1e6
        twist[0] = self.vx          # vx
        twist[7] = self.vx          # vy (skid-steer: should be ~0)
        twist[35] = self.vyaw       # vyaw
        m.pose.covariance = pose
        m.twist.covariance = twist
        self.pub.publish(m)


def main():
    rclpy.init()
    node = OdomCovRelay()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == '__main__':
    main()