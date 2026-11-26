#!/usr/bin/env python3
"""
key_drive: smooth keyboard driving for the Auracle (replaces teleop_twist_keyboard).

Publishes   cmd_vel  (geometry_msgs/Twist)  -> run with -r cmd_vel:=cmd_vel_joy

WHY NOT teleop_twist_keyboard
  It publishes one step command per key event, with no ramp. Holding a key relies on the
  OS key repeat, whose first delay (~0.5 s) is as long as twist_mux's and diff_cont's
  0.5 s timeouts, so the robot went go-stop-go and stopped dead on release.

HOW THIS ONE WORKS
  A terminal can't see key-up, only repeats. A key counts as held for `first_hold` s after
  the first press (covers the repeat delay), then for `repeat_hold` s after each repeat, so
  letting go is noticed ~0.15 s later. The target then goes through the same two cascaded
  low-pass filters and slew limits as joy_drive.py, and is published at a fixed rate, so
  starts, stops and turns are ramps. Hold one key at a time.

EVERY KEY IS HOLD-TO-MOVE
  The robot moves only while a key is held, and eases to a stop when you let go.
  A terminal only repeats the LAST key held, so holding w and tapping d can't come back to
  straight when d is released (no more w's arrive). Use u / o to drive and turn together.

KEYS (hold)
  w / i        forward                      s / ,   backward
  a / j        spin left                    d / l   spin right
  u / o        forward arc left / right     m / .   backward arc left / right
  space / k    stop now (smoothly)          q / z   speed level +10 % / -10 %
  Ctrl+C       quit (sends zero)
"""
import math
import select
import sys
import termios
import tty

import rclpy
from geometry_msgs.msg import Twist
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node

# (linear, angular) direction for each key.
# (drive, turn) while the key is held.
BINDINGS = {
    'w': (1, 0), 'i': (1, 0), 's': (-1, 0), ',': (-1, 0),
    'a': (0, 1), 'j': (0, 1), 'd': (0, -1), 'l': (0, -1),
    'u': (1, 1), 'o': (1, -1), 'm': (-1, -1), '.': (-1, 1),
}
STOP_KEYS = (' ', 'k')

DEFAULTS = {
    'max_linear': 0.16,          # m/s forward (my_controllers_real.yaml clamps at 0.16: weak right rear)
    'max_linear_reverse': 0.25,  # m/s backward (right rear is healthy backward)
    'max_angular': 1.3,          # rad/s for spins
    'arc_turn_scale': 0.11,      # arc turn rate = this * max_angular (~0.14 rad/s). Arcs top out at ~0.16 rad/s
                                 # (weak right rear); more than that becomes a spin.
    'initial_speed': 1.0,        # 100 % = 0.14 m/s; below ~70 % the Nano rounds up to its 0.10 m/s minimum
    'speed_min': 0.7,
    'speed_max': 1.0,
    'speed_step': 0.1,
    'publish_rate': 40.0,        # Hz, matches controller_manager on the robot (40)
    'first_hold': 0.6,           # s a single press counts as held (covers the key-repeat delay)
    'repeat_hold': 0.35,         # s after each repeat. Was 0.15: key repeats over SSH arrive in
                                 # bunches with gaps, which read as release/press = bursts.
    'smoothing_linear': 0.07,    # s, time constant of EACH of the two filters (as joy_drive)
    'smoothing_angular': 0.09,
    'decel_ratio': 0.7,
    'max_accel_linear': 0.55,    # m/s^2   keep below diff_cont limit (0.6)
    'max_accel_angular': 2.8,    # rad/s^2 keep below diff_cont limit (3.0)
}


def clamp(x, lo, hi):
    return lo if x < lo else hi if x > hi else x


def lag(state, target, tau, dt):
    """First-order low-pass, exact for any dt."""
    if tau <= 1e-6:
        return target
    return state + (target - state) * (1.0 - math.exp(-dt / tau))


class KeyMixer:
    """Key state -> smooth (linear, angular). No ROS in it, so it can be tested offline."""

    def __init__(self, cfg):
        self.c = dict(cfg)
        self.speed = clamp(cfg['initial_speed'], cfg['speed_min'], cfg['speed_max'])
        self.key = None         # key currently held
        self.key_until = 0.0    # held until this time unless another repeat arrives
        self.l1 = self.l2 = self.lin = 0.0
        self.a1 = self.a2 = self.ang = 0.0
        self.sent_zero = True

    def press(self, ch, now):
        """Returns a message to show, or None."""
        c = self.c
        if ch in ('q', 'z'):
            step = c['speed_step'] if ch == 'q' else -c['speed_step']
            self.speed = clamp(round(self.speed + step, 3), c['speed_min'], c['speed_max'])
            return f'Speed level {round(self.speed * 100)}%'
        if ch in STOP_KEYS:
            self.key = None
            return None
        if ch in BINDINGS:
            repeat = ch == self.key and now < self.key_until
            self.key = ch
            self.key_until = now + (c['repeat_hold'] if repeat else c['first_hold'])
        return None

    def _targets(self, now):
        c = self.c
        if self.key is None or now >= self.key_until:
            self.key = None
            return 0.0, 0.0
        drive, turn = BINDINGS[self.key]
        rate = c['max_angular'] * (c['arc_turn_scale'] if drive != 0 else 1.0)
        lin_max = c['max_linear'] if drive >= 0 else c['max_linear_reverse']
        return drive * lin_max * self.speed, turn * rate * self.speed

    def step(self, now, dt):
        """Returns (linear, angular, publish?)."""
        c = self.c
        t_lin, t_ang = self._targets(now)
        dr = c['decel_ratio']

        def tau(base, target, state):
            return base * dr if abs(target) < abs(state) else base

        tl, ta = c['smoothing_linear'], c['smoothing_angular']
        self.l1 = lag(self.l1, t_lin, tau(tl, t_lin, self.l1), dt)
        self.l2 = lag(self.l2, self.l1, tau(tl, self.l1, self.l2), dt)
        self.a1 = lag(self.a1, t_ang, tau(ta, t_ang, self.a1), dt)
        self.a2 = lag(self.a2, self.a1, tau(ta, self.a1, self.a2), dt)

        m = c['max_accel_linear'] * dt
        self.lin += clamp(self.l2 - self.lin, -m, m)
        m = c['max_accel_angular'] * dt
        self.ang += clamp(self.a2 - self.ang, -m, m)

        # Snap the exponential tail to exactly zero so we can go quiet.
        if t_lin == 0.0 and max(abs(self.l1), abs(self.l2), abs(self.lin)) < 0.004:
            self.l1 = self.l2 = self.lin = 0.0
        if t_ang == 0.0 and max(abs(self.a1), abs(self.a2), abs(self.ang)) < 0.02:
            self.a1 = self.a2 = self.ang = 0.0

        if t_lin != 0.0 or t_ang != 0.0 or self.lin != 0.0 or self.ang != 0.0:
            self.sent_zero = False
            return self.lin + 0.0, self.ang + 0.0, True
        if not self.sent_zero:           # one explicit zero, then silence (twist_mux releases)
            self.sent_zero = True
            return 0.0, 0.0, True
        return 0.0, 0.0, False


class KeyDriveNode(Node):
    def __init__(self):
        super().__init__('key_drive_node')
        cfg = {}
        for key, default in DEFAULTS.items():
            self.declare_parameter(key, default)
            cfg[key] = self.get_parameter(key).value
        self.mixer = KeyMixer(cfg)
        self.pub = self.create_publisher(Twist, 'cmd_vel', 10)
        self.period = 1.0 / cfg['publish_rate']

    def now_s(self):
        return self.get_clock().now().nanoseconds * 1e-9

    def publish(self, lin, ang):
        msg = Twist()
        msg.linear.x = float(lin)
        msg.angular.z = float(ang)
        self.pub.publish(msg)

    def run(self):
        print(__doc__.split('KEYS')[1].rstrip(), flush=True)
        print(f'\nSpeed level {round(self.mixer.speed * 100)}%', flush=True)
        last = self.now_s()
        next_tick = last
        while rclpy.ok():
            timeout = max(0.0, next_tick - self.now_s())
            ready, _, _ = select.select([sys.stdin], [], [], timeout)
            now = self.now_s()
            if ready:
                ch = sys.stdin.read(1)
                if ch == '\x03':         # Ctrl+C in raw mode
                    break
                msg = self.mixer.press(ch, now)
                if msg:
                    print(f'\r{msg}        ', flush=True)
            if now >= next_tick:
                dt = min(now - last, 0.1)
                last = now
                next_tick = now + self.period
                lin, ang, publish = self.mixer.step(now, dt)
                if publish:
                    self.publish(lin, ang)


def main():
    if not sys.stdin.isatty():
        print('key_drive needs an interactive terminal (run it with ros2 run, not ros2 launch).')
        return
    rclpy.init()
    node = KeyDriveNode()
    settings = termios.tcgetattr(sys.stdin)
    try:
        tty.setcbreak(sys.stdin.fileno())
        new = termios.tcgetattr(sys.stdin)
        new[3] &= ~termios.ISIG          # deliver Ctrl+C as a key so we can stop cleanly
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, new)
        node.run()
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, settings)
        try:
            node.publish(0.0, 0.0)
        except Exception:
            pass
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == '__main__':
    main()
