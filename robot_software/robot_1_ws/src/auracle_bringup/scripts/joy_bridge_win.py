"""
Windows side: read a controller with pygame, send it over UDP to WSL.

Run in Windows (not WSL):
    pip install pygame
    python joy_bridge_win.py --probe            # TEST MODE: shows what each input maps to
    python joy_bridge_win.py                    # send to WSL (127.0.0.1:5005)
    python joy_bridge_win.py --host 172.x.x.x   # if localhost doesn't reach WSL

Output is remapped to what Linux joy_node reports for an Xbox pad, so
joystick.yaml works unchanged:
    axes:    [LX, LY, LT, RX, RY, RT, dpadX, dpadY]
             stick left/up = +1, triggers rest at +1 and go to -1 when pressed
    buttons: [A, B, X, Y, LB, RB, Back, Start, Guide, L3, R3]

MAPPING below was filled in from a probe of an 8BitDo Ultimate 2C over
Bluetooth (Windows exposes it as a generic 6-axis / 16-button pad):
  - left stick  = axis 0 (X), axis 1 (Y)   [axis 2 duplicates axis 1]
  - right stick = axis 3 (X), axis 4 (Y)   [axis 5 duplicates axis 4]
  - A=0 B=1 X=3 Y=4 LB=6 RB=7, LT=8 RT=9 (triggers are BUTTONS on this pad)
  - Back/Start/Guide/L3/R3 = 10..14 is a GUESS: confirm with --probe.
Stick signs assume pygame's usual right/down = +1 (flipped to Linux's
left/up = +1). Confirm with --probe: pushing a stick UP must show +.
"""
import argparse
import json
import socket
import time

import pygame

DEADZONE = 0.05

AXIS_NAMES = ["LX", "LY", "LT", "RX", "RY", "RT"]
BUTTON_NAMES = ["A", "B", "X", "Y", "LB", "RB", "Back", "Start", "Guide", "L3", "R3"]

# One entry per name in AXIS_NAMES:
#   ("axis", pygame_axis_index, sign)  stick axis; sign=-1 flips direction
#   ("btn",  pygame_button_index)      trigger that the pad reports as a button
#   None                               not present (sends rest value)
LINUX_AXES = [
    ("axis", 0, -1),   # LX
    ("axis", 1, -1),   # LY
    ("btn", 8),        # LT
    ("axis", 3, -1),   # RX
    ("axis", 4, -1),   # RY
    ("btn", 9),        # RT
]

# pygame button index for each name in BUTTON_NAMES (None = not present)
LINUX_BUTTONS = [0, 1, 3, 4, 6, 7, 10, 11, 12, 13, 14]

# Duplicate axes this pad reports; hidden in probe output.
IGNORE_AXES = {2, 5}


def dz(v):
    return 0.0 if abs(v) < DEADZONE else v


def label_button(i):
    if i in LINUX_BUTTONS:
        return BUTTON_NAMES[LINUX_BUTTONS.index(i)]
    for name, spec in zip(AXIS_NAMES, LINUX_AXES):
        if spec and spec[0] == "btn" and spec[1] == i:
            return f"{name} (trigger)"
    return "UNMAPPED"


def label_axis(i, v):
    for name, spec in zip(AXIS_NAMES, LINUX_AXES):
        if spec and spec[0] == "axis" and spec[1] == i:
            return f"{name} = {spec[2] * v:+.2f}   (Linux value)"
    return "UNMAPPED"


def probe(js):
    """Test mode: print each input and what the bridge will send for it."""
    n = js.get_numaxes()
    last = [js.get_axis(i) for i in range(n)]
    print("TEST MODE (nothing is sent). Press/move one thing at a time.")
    print("Expected: stick UP -> LY positive, stick LEFT -> LX positive.")
    print("Check every button shows the name you pressed. Ctrl-C to stop.\n")
    while True:
        for e in pygame.event.get():
            if e.type == pygame.JOYBUTTONDOWN:
                print(f"button {e.button:2d} DOWN  -> {label_button(e.button)}")
            elif e.type == pygame.JOYBUTTONUP:
                print(f"button {e.button:2d} up")
            elif e.type == pygame.JOYHATMOTION:
                x, y = e.value
                print(f"hat {e.hat} = {e.value}  -> dpadX = {-x:+d}, dpadY = {y:+d}")
        for i in range(n):
            if i in IGNORE_AXES:
                continue
            v = js.get_axis(i)
            if abs(v - last[i]) > 0.25:
                print(f"axis {i} = {v:+.2f}  -> {label_axis(i, v)}")
                last[i] = v
        time.sleep(0.02)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=5005)
    ap.add_argument("--rate", type=float, default=50.0, help="packets per second")
    ap.add_argument("--index", type=int, default=0, help="which controller")
    ap.add_argument("--probe", action="store_true", help="test mode: print events, send nothing")
    ap.add_argument("--raw", action="store_true", help="also print the values being sent")
    args = ap.parse_args()

    pygame.init()
    pygame.joystick.init()
    if pygame.joystick.get_count() == 0:
        raise SystemExit("No controller found. Pair it to Windows (or plug it in) first.")
    js = pygame.joystick.Joystick(args.index)
    js.init()
    print(f"Using: {js.get_name()}  axes={js.get_numaxes()} "
          f"buttons={js.get_numbuttons()} hats={js.get_numhats()}")

    try:
        if args.probe:
            probe(js)
            return

        print(f"Sending to {args.host}:{args.port} at {args.rate} Hz. Ctrl-C to stop.")
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        dest = (args.host, args.port)
        period = 1.0 / args.rate

        def ax(i):
            return js.get_axis(i) if i < js.get_numaxes() else 0.0

        def btn(i):
            if i is None or i >= js.get_numbuttons():
                return 0
            return js.get_button(i)

        while True:
            pygame.event.pump()

            axes = []
            for spec in LINUX_AXES:
                if spec is None:
                    axes.append(0.0)
                elif spec[0] == "axis":
                    axes.append(dz(spec[2] * ax(spec[1])) + 0.0)  # "+ 0.0" turns -0.0 into 0.0
                else:  # trigger as button: rest +1, pressed -1 (Linux convention)
                    axes.append(1.0 - 2.0 * btn(spec[1]))

            hat = js.get_hat(0) if js.get_numhats() > 0 else (0, 0)
            axes += [float(-hat[0]) + 0.0, float(hat[1]) + 0.0]  # dpad: left/up = +1

            buttons = [btn(i) for i in LINUX_BUTTONS]

            sock.sendto(json.dumps({"axes": axes, "buttons": buttons}).encode(), dest)

            if args.raw:
                print([round(a, 2) for a in axes], buttons, end="\r")

            time.sleep(period)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()