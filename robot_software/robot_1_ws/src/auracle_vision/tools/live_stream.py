"""Live accuracy check: Pi camera stream -> waste model on this PC -> window with predictions.

Point the Pi camera at your phone screen, show one image at a time, and press a key to say what
the image really is. The script tallies per-frame and per-vote results and prints confusion
matrices when you quit.

    python3 live_stream.py http://auracle.local:8090/<stream path>
    python3 live_stream.py http://auracle.local:8090/<stream path> --clahe --roi 0.1,0.1,0.9,0.9

Keys:  m = metal   o = organic   p = plastic   x = clear label   q = quit
Needs: pip install opencv-python numpy onnxruntime
Run from the repo root, or pass --pkg <path to src/auracle_vision>.
"""
import argparse
import os
import sys
import time
from collections import Counter

import cv2

ap = argparse.ArgumentParser()
ap.add_argument('url', help='MJPEG stream URL (or a camera index / video file)')
ap.add_argument('--pkg', default='src/auracle_vision')
ap.add_argument('--model', default='waste_yolo26n_320.onnx')
ap.add_argument('--clahe', action='store_true')
ap.add_argument('--roi', default='0,0,1,1', help='crop x0,y0,x1,y1 as fractions of the frame')
ap.add_argument('--rate', type=float, default=5.0, help='inferences per second (robot: 5)')
a = ap.parse_args()

sys.path.insert(0, a.pkg)
from auracle_vision.waste_model import Voter, WasteModel  # noqa: E402

model_path = a.model if os.path.isabs(a.model) else os.path.join(a.pkg, 'models', a.model)
model = WasteModel(model_path, threads=2, conf=0.25, clahe=a.clahe)
voter = Voter(10, 6, 0.35)          # same vote rule as the robot
x0, y0, x1, y1 = [float(v) for v in a.roi.split(',')]

src = int(a.url) if a.url.isdigit() else a.url
cap = cv2.VideoCapture(src)
if not cap.isOpened():
    sys.exit(f'could not open {a.url}')

KEYS = {ord('m'): 'metal', ord('o'): 'organic', ord('p'): 'plastic', ord('x'): None}
truth = None
frame_cm, vote_cm = Counter(), Counter()
last_decision, best, status = '', None, ''
last_run = 0.0

while True:
    ok, img = cap.read()      # read every frame so the stream buffer never backs up
    if not ok:
        print('stream ended')
        break
    now = time.time()
    if now - last_run >= 1.0 / a.rate:
        last_run = now
        h, w = img.shape[:2]
        ox, oy = int(x0 * w), int(y0 * h)
        crop = img[oy:int(y1 * h), ox:int(x1 * w)]
        t0 = time.perf_counter()
        dets = model.detect(crop)
        ms = (time.perf_counter() - t0) * 1000
        best = (dets[0][0], dets[0][1]) if dets else None
        if truth is not None:
            frame_cm[(truth, best[0] if best else 'none')] += 1
        decision = voter.add(best)
        if decision:
            label, n, score = decision
            last_decision = f'{label} ({n}/10, {score:.2f})'
            if truth is not None:
                vote_cm[(truth, label)] += 1
            voter.reset()
        status = f'{dict(voter.tally())}  {ms:.0f} ms'
        for name, score, (bx, by, bw, bh) in dets[:3]:
            p1 = (int(ox + bx), int(oy + by))
            p2 = (int(ox + bx + bw), int(oy + by + bh))
            cv2.rectangle(img, p1, p2, (0, 255, 0), 2)
            cv2.putText(img, f'{name} {score:.2f}', (p1[0], max(p1[1] - 6, 12)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
        cv2.rectangle(img, (ox, oy), (int(x1 * w), int(y1 * h)), (255, 128, 0), 1)
    lines = [f'truth: {truth or "(press m / o / p)"}',
             f'frame: {best[0] + f" {best[1]:.2f}" if best else "none"}',
             f'votes: {status}',
             f'last decision: {last_decision or "-"}']
    for i, t in enumerate(lines):
        cv2.putText(img, t, (8, 22 + 22 * i), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 0), 4)
        cv2.putText(img, t, (8, 22 + 22 * i), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
    cv2.imshow('waste model live test', img)
    k = cv2.waitKey(1) & 0xFF
    if k == ord('q'):
        break
    if k in KEYS:
        truth = KEYS[k]
        voter.reset()
        last_decision = ''


def report(title, cm):
    total = sum(cm.values())
    print(f'\n{title} (n={total})')
    if not total:
        print('  nothing recorded: press m / o / p to set the true label first')
        return
    for t in ('metal', 'organic', 'plastic'):
        row = {p: n for (tt, p), n in cm.items() if tt == t}
        n = sum(row.values())
        if n:
            print(f'  true {t:8s} n={n:4d}  right {row.get(t, 0) / n:6.1%}   {row}')
    right = sum(n for (t, p), n in cm.items() if t == p)
    print(f'  overall right: {right / total:.1%}')


report('Per-frame (best detection, score >= 0.25; "none" = nothing found)', frame_cm)
report('Vote decisions (what the robot would act on)', vote_cm)
cap.release()
cv2.destroyAllWindows()