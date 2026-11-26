#!/usr/bin/env python3
"""Record LOAD 1-3 and OFFLOAD 1-3 by driving the real robot to each spot.

Terminal A: localisation on your map (AMCL running, pose set), e.g.
    ros2 launch auracle_bringup localization_nav_rviz.launch.py map:=$HOME/maps/trial_6a.yaml ...
Terminal B: drive with joy_drive / key_drive.
Terminal C (this script):
    python3 collect_regions.py --out ~/maps/trial_6a_regions.yaml

For each point it asks you to drive there. When the robot sits exactly where you want it (and
points the way you want it to end up), press Enter: the robot's pose in the map frame (x, y, yaw)
is saved. For an offload bay you enter in reverse, the saved yaw is the direction the FRONT of
the robot points, which is what the file expects.

Keys at each prompt:  Enter = record   s = skip (keep the old value)   r = show pose, no record   q = quit
The file is rewritten after every point, so a crash loses nothing. start: and home: are copied
from the existing file (or default to 0,0,0). A .bak copy of the old file is kept.
Needs only rclpy + tf2_ros (a sourced ROS 2 shell). No build step.
"""
import argparse, math, os, re, shutil, sys, threading, time

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, DurabilityPolicy, ReliabilityPolicy
from tf2_msgs.msg import TFMessage
import tf2_ros

KEYS = ['load_1', 'load_2', 'load_3', 'offload_1', 'offload_2', 'offload_3']
LABEL = {'load': 'LOADING', 'offload': 'OFFLOADING'}


class PoseReader(Node):
    def __init__(self, ns, map_frame, base_frame):
        super().__init__('collect_regions')
        self.map_frame, self.base_frame = map_frame, base_frame
        self.buf = tf2_ros.Buffer(cache_time=rclpy.duration.Duration(seconds=30))
        p = f'/{ns}' if ns else ''
        dyn = QoSProfile(depth=100, reliability=ReliabilityPolicy.RELIABLE)
        stat = QoSProfile(depth=100, reliability=ReliabilityPolicy.RELIABLE,
                          durability=DurabilityPolicy.TRANSIENT_LOCAL)
        self.create_subscription(TFMessage, f'{p}/tf', lambda m: self._tf(m, False), dyn)
        self.create_subscription(TFMessage, f'{p}/tf_static', lambda m: self._tf(m, True), stat)

    def _tf(self, msg, static):
        for t in msg.transforms:
            try:
                self.buf.set_transform_static(t, 'collect') if static else self.buf.set_transform(t, 'collect')
            except Exception:
                pass

    def pose(self, tries=20):
        """(x, y, yaw) of base in map, from the latest transform. None if not available."""
        for _ in range(tries):
            try:
                t = self.buf.lookup_transform(self.map_frame, self.base_frame, rclpy.time.Time())
                q = t.transform.rotation
                yaw = math.atan2(2 * (q.w * q.z + q.x * q.y), 1 - 2 * (q.y * q.y + q.z * q.z))
                age = (self.get_clock().now() - rclpy.time.Time.from_msg(t.header.stamp)).nanoseconds / 1e9
                return t.transform.translation.x, t.transform.translation.y, yaw, age
            except Exception:
                time.sleep(0.1)
        return None


def read_old(path):
    """Existing entries as raw lines per key, so start/home and unchanged points are kept as written."""
    old = {}
    if os.path.exists(path):
        key = None
        for line in open(path):
            m = re.match(r'^([A-Za-z_0-9]+):\s*(.*)$', line.rstrip('\n'))
            if m and not line.startswith((' ', '\t', '#')):
                key = m[1]; old[key] = {'inline': m[2], 'items': []}
            elif key and line.strip().startswith('-'):
                old[key]['items'].append(line.strip())
    return old


def write(path, old, new):
    lines = ['# Regions in the map frame (metres, radians). Recorded by driving the robot there',
             '# (collect_regions.py): yaw is the direction the FRONT of the robot points when it stops.']
    def block(k, default):
        if k in new:
            x, y, yaw = new[k]
            return [f'{k}:', f'  - {{x: {x:.3f}, y: {y:.3f}, yaw: {yaw:.3f}}}']
        o = old.get(k)
        if o and o['items']:
            return [f'{k}:'] + ['  ' + i for i in o['items']]
        if o and o['inline'] and not o['inline'].startswith('[]') and o['inline']:
            return [f'{k}: {o["inline"]}']
        return default
    lines += block('start', ['start: {x: 0.0, y: 0.0, yaw: 0.0}'])
    lines += block('home', ['home:', '  - {x: 0.0, y: 0.0, yaw: 0.0}'])
    for k in KEYS:
        lines += block(k, [f'{k}: []'])
    tmp = path + '.tmp'
    open(tmp, 'w').write('\n'.join(lines) + '\n')
    os.replace(tmp, path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=os.path.expanduser('~/maps/trial_6a_regions.yaml'))
    ap.add_argument('--namespace', default='auracle')
    ap.add_argument('--map-frame', default='map')
    ap.add_argument('--base-frame', default='base_footprint')
    ap.add_argument('--only', nargs='*', choices=KEYS, help='record just these, e.g. --only offload_1')
    a = ap.parse_args()
    out = os.path.expanduser(a.out)

    rclpy.init()
    node = PoseReader(a.namespace.strip('/'), a.map_frame, a.base_frame)
    threading.Thread(target=rclpy.spin, args=(node,), daemon=True).start()

    print(f'Reading {a.map_frame} -> {a.base_frame} on /{a.namespace}/tf ...')
    p = node.pose(tries=50)
    if p is None:
        sys.exit(f'No {a.map_frame}->{a.base_frame} transform. Is localisation running and the initial pose set?')
    print(f'OK, robot at x={p[0]:.2f} y={p[1]:.2f} yaw={math.degrees(p[2]):.0f} deg\n')

    old = read_old(out)
    if os.path.exists(out):
        shutil.copy(out, out + '.bak')
    new = {}
    for key in (a.only or KEYS):
        kind, n = key.split('_')
        print(f'--- {LABEL[kind]} {n} ---')
        while True:
            ans = input(f'Drive to {key}, then Enter to record [s=skip, r=show, q=quit]: ').strip().lower()
            if ans == 'q':
                print('Stopped. Saved so far:', out); rclpy.shutdown(); return
            if ans == 's':
                print('  skipped, old value kept\n'); break
            p = node.pose(tries=5)
            if p is None:
                print('  no transform right now, try again'); continue
            x, y, yaw, age = p
            show = f'x={x:.3f} y={y:.3f} yaw={yaw:.3f} rad ({math.degrees(yaw):.0f} deg)'
            if age > 1.0:
                print(f'  WARNING: pose is {age:.1f} s old (is AMCL still publishing?)')
            if ans == 'r':
                print('  ' + show); continue
            new[key] = (x, y, yaw)
            write(out, old, new)
            print(f'  saved {key}: {show}\n')
            break
    print('Done:', out)
    rclpy.shutdown()


if __name__ == '__main__':
    main()