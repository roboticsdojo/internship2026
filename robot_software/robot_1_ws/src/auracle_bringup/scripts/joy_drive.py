#!/usr/bin/env python3
"""
joy_drive: game-style gamepad driving for the Auracle (replaces teleop_twist_joy).

Subscribes  joy      (sensor_msgs/Joy)
Publishes   cmd_vel  (geometry_msgs/Twist)  -> launch remaps this to cmd_vel_joy

CONTROLS (Linux xpad numbering; every index is a parameter in joystick.yaml)
  LT / RT        left / right "track". Both pressed = forward. Release LT and the
                 robot banks left (right side faster); release RT and it banks
                 right. Hold Back to flip both tracks to reverse.
  D-pad up/down  speed level up/down (10 % steps, hold to repeat). Scales EVERYTHING.
  Y / A          forward / backward          (digital, ramped smoothly)
  X / B          turn left / turn right      (combine with Y/A for arcs)
  LB + left stick  analog driving (LB is the dead-man for the stick only)

All three schemes are live at once. For each of forward and turn, the input with
the largest magnitude wins, so they never add up or fight. Turn schemes off with
`enabled_modes` in the yaml.

SMOOTHNESS
  Targets go through two cascaded low-pass filters (an S-curve start: acceleration
  begins at zero, no sudden kick), then a slew-rate limiter set BELOW diff_cont's
  own acceleration limits so diff_cont never has to clamp (clamping is what makes
  motion feel steppy). Braking uses a slightly shorter time constant than
  accelerating. Output is a fixed-rate stream, independent of how fast /joy arrives.

SAFETY
  - No /joy for `stale_timeout` -> target becomes zero and the robot ramps to a stop.
  - A trigger must be seen at rest once before it is trusted (some drivers report
    0 = "half pressed" until first touched).
  - Publishes only while engaged or still slowing down, then one final zero and
    silence, so twist_mux releases to Nav2 (priority 50) when you let go.
"""
import math

import rclpy
from geometry_msgs.msg import Twist
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from sensor_msgs.msg import Joy

# Types matter: ints stay ints and floats stay floats, so yaml values must match.
DEFAULTS = {
    'enabled_modes': ['stick', 'triggers', 'buttons'],

    # --- speed at 100 % speed level ---
    'max_linear': 0.25,          # m/s   (real robot clamps at 0.25 in my_controllers_real.yaml)
    'max_linear_reverse': 0.25,  # m/s backward (can differ from forward)
    'max_angular': 1.5,          # rad/s
    'wheel_separation': 0.2755,  # m     used for trigger banking; match diff_cont

    # --- d-pad speed levels ---
    'initial_speed': 0.5,
    'speed_min': 0.1,
    'speed_max': 1.0,
    'speed_step': 0.1,
    'speed_repeat_delay': 0.4,   # s before holding the d-pad starts repeating
    'speed_repeat_rate': 6.0,    # steps per second while held

    # --- smoothing ---
    'publish_rate': 50.0,        # Hz
    'smoothing_linear': 0.12,    # s, time constant of EACH of the two filters
    'smoothing_angular': 0.15,   # bigger = smoother but lazier; ~4x this to reach 90 %
    'decel_ratio': 0.7,          # braking time constant = ratio * accel time constant
    'max_accel_linear': 0.55,    # m/s^2   keep below diff_cont limit (0.6)
    'max_accel_angular': 2.8,    # rad/s^2 keep below diff_cont limit (3.0)
    'stale_timeout': 0.3,        # s without /joy -> stop

    # --- input shaping ---
    'stick_deadzone': 0.08,
    'stick_expo': 1.5,           # >1 = finer control near centre
    'trigger_deadzone': 0.03,
    'trigger_expo': 1.5,
    'button_turn_scale': 0.7,    # X/B turn rate relative to full

    # --- mapping (Linux xpad layout, what joy_bridge_win.py also sends) ---
    'axis_stick_x': 0,
    'axis_stick_y': 1,
    'axis_lt': 2,
    'axis_rt': 5,
    'axis_dpad_y': 7,
    'trigger_rest': 1.0,         # raw value when released
    'trigger_pressed': -1.0,     # raw value when fully pressed
    'button_enable': 4,          # LB
    'button_reverse': 6,         # Back
    'button_forward': 3,         # Y
    'button_back': 0,            # A
    'button_left': 2,            # X
    'button_right': 1,           # B
}


def clamp(x, lo, hi):
    return lo if x < lo else hi if x > hi else x


def shape(x, deadzone, expo):
    """Deadzone (rescaled so output still reaches 1.0) then a power curve."""
    a = abs(x)
    if a <= deadzone:
        return 0.0
    a = min((a - deadzone) / (1.0 - deadzone), 1.0)
    return math.copysign(a ** expo, x)


def lag(state, target, tau, dt):
    """First-order low-pass, exact for any dt."""
    if tau <= 1e-6:
        return target
    return state + (target - state) * (1.0 - math.exp(-dt / tau))


def pick(values):
    """The value with the largest magnitude (0.0 if the list is empty)."""
    best = 0.0
    for v in values:
        if abs(v) > abs(best):
            best = v
    return best


class DriveMixer:
    """All the driving logic, with no ROS in it so it can be tested offline."""

    def __init__(self, cfg):
        self.c = dict(cfg)
        c = self.c
        self.speed = clamp(c['initial_speed'], c['speed_min'], c['speed_max'])
        self.axes = []
        self.buttons = []
        self.joy_time = None
        self.trig_armed = {'lt': False, 'rt': False}
        self.trig_warned = set()
        self.dpad_dir = 0
        self.dpad_since = 0.0
        self.dpad_last = 0.0
        self.l1 = self.l2 = self.lin = 0.0   # linear: two filters + limited output
        self.a1 = self.a2 = self.ang = 0.0   # angular
        self.sent_zero = True
        self.was_stale = True
        self.events = []

    # ----- input -----
    def set_joy(self, axes, buttons, now):
        self.axes = axes
        self.buttons = buttons
        self.joy_time = now

    def pop_events(self):
        ev, self.events = self.events, []
        return ev

    def _axis(self, i):
        return self.axes[i] if 0 <= i < len(self.axes) else 0.0

    def _btn(self, i):
        return 1 if (0 <= i < len(self.buttons) and self.buttons[i]) else 0

    def _trigger(self, name, idx):
        c = self.c
        if not 0 <= idx < len(self.axes):
            return 0.0
        raw = self.axes[idx]
        rest, pressed = c['trigger_rest'], c['trigger_pressed']
        if not self.trig_armed[name]:
            if abs(raw - rest) < 0.1:
                self.trig_armed[name] = True
            else:
                if name not in self.trig_warned:
                    self.trig_warned.add(name)
                    self.events.append(
                        f'Trigger {name.upper()} reads {raw:+.2f} (rest should be '
                        f'{rest:+.1f}); ignoring it until it has been at rest once')
                return 0.0
        t = clamp((raw - rest) / (pressed - rest), 0.0, 1.0)
        return shape(t, c['trigger_deadzone'], c['trigger_expo'])

    # ----- d-pad speed levels -----
    def _bump_speed(self, direction):
        c = self.c
        new = clamp(round(self.speed + direction * c['speed_step'], 3),
                    c['speed_min'], c['speed_max'])
        if new != self.speed:
            self.speed = new
            self.events.append(f'Speed level {round(new * 100)}%')

    def _update_speed(self, now):
        c = self.c
        y = self._axis(c['axis_dpad_y'])
        d = 1 if y > 0.5 else -1 if y < -0.5 else 0
        if d != self.dpad_dir:
            self.dpad_dir = d
            self.dpad_since = self.dpad_last = now
            if d != 0:
                self._bump_speed(d)
        elif d != 0:
            if (now - self.dpad_since > c['speed_repeat_delay']
                    and now - self.dpad_last >= 1.0 / c['speed_repeat_rate']):
                self._bump_speed(d)
                self.dpad_last = now

    # ----- turn inputs into a target (linear m/s, angular rad/s) -----
    def _targets(self):
        c = self.c
        s = self.speed
        modes = c['enabled_modes']
        lins, angs = [], []

        if 'stick' in modes and self._btn(c['button_enable']):
            lins.append(shape(self._axis(c['axis_stick_y']),
                              c['stick_deadzone'], c['stick_expo']) * c['max_linear'] * s)
            angs.append(shape(self._axis(c['axis_stick_x']),
                              c['stick_deadzone'], c['stick_expo']) * c['max_angular'] * s)

        if 'triggers' in modes:
            a = self._trigger('lt', c['axis_lt'])
            b = self._trigger('rt', c['axis_rt'])
            sign = -1.0 if self._btn(c['button_reverse']) else 1.0
            vl = sign * a * c['max_linear'] * s      # left track speed
            vr = sign * b * c['max_linear'] * s      # right track speed
            lins.append((vl + vr) / 2.0)
            angs.append(clamp((vr - vl) / c['wheel_separation'],
                              -c['max_angular'], c['max_angular']))

        if 'buttons' in modes:
            lins.append((self._btn(c['button_forward']) - self._btn(c['button_back']))
                        * c['max_linear'] * s)
            angs.append((self._btn(c['button_left']) - self._btn(c['button_right']))
                        * c['max_angular'] * s * c['button_turn_scale'])

        lin = pick(lins)
        if lin < 0.0:   # every scheme above scales by max_linear; backward may be faster
            lin *= c['max_linear_reverse'] / c['max_linear']
        return lin, pick(angs)

    # ----- one control tick -----
    def step(self, now, dt):
        """Returns (linear, angular, publish?)."""
        c = self.c
        fresh = self.joy_time is not None and (now - self.joy_time) <= c['stale_timeout']
        if fresh:
            if self.was_stale:
                self.events.append(f'Controller input active (speed {round(self.speed * 100)}%)')
                self.was_stale = False
            self._update_speed(now)
            t_lin, t_ang = self._targets()
        else:
            if not self.was_stale:
                self.events.append(f'No /joy for {c["stale_timeout"]}s: stopping')
                self.was_stale = True
            self.dpad_dir = 0
            t_lin = t_ang = 0.0

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

        engaged = t_lin != 0.0 or t_ang != 0.0
        moving = self.lin != 0.0 or self.ang != 0.0
        if engaged or moving:
            self.sent_zero = False
            return self.lin + 0.0, self.ang + 0.0, True
        if not self.sent_zero:           # one explicit zero, then silence
            self.sent_zero = True
            return 0.0, 0.0, True
        return 0.0, 0.0, False


class JoyDriveNode(Node):
    def __init__(self):
        super().__init__('joy_drive_node')
        cfg = {}
        for key, default in DEFAULTS.items():
            self.declare_parameter(key, default)
            cfg[key] = self.get_parameter(key).value
        self.mixer = DriveMixer(cfg)
        self.last_t = None

        self.pub = self.create_publisher(Twist, 'cmd_vel', 10)
        self.create_subscription(Joy, 'joy', self.on_joy, 10)
        self.create_timer(1.0 / cfg['publish_rate'], self.on_timer)
        self.get_logger().info(
            f'joy_drive ready: modes={cfg["enabled_modes"]}, '
            f'max {cfg["max_linear"]} m/s / {cfg["max_angular"]} rad/s, '
            f'speed level {round(self.mixer.speed * 100)}%')

    def now_s(self):
        return self.get_clock().now().nanoseconds * 1e-9

    def on_joy(self, msg):
        self.mixer.set_joy(list(msg.axes), list(msg.buttons), self.now_s())

    def on_timer(self):
        now = self.now_s()
        if self.last_t is None:
            self.last_t = now
            return
        dt = now - self.last_t
        if dt <= 0.0:
            return
        self.last_t = now
        lin, ang, publish = self.mixer.step(now, min(dt, 0.1))
        for text in self.mixer.pop_events():
            self.get_logger().info(text)
        if publish:
            self.publish(lin, ang)

    def publish(self, lin, ang):
        msg = Twist()
        msg.linear.x = float(lin)
        msg.angular.z = float(ang)
        self.pub.publish(msg)

    def publish_zero(self):
        self.publish(0.0, 0.0)


def main():
    rclpy.init()
    node = JoyDriveNode()
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        try:
            node.publish_zero()
        except Exception:
            pass
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == '__main__':
    main()