"""Collect a small labelled test set from what is in the repos: the 4x2 sample montages (all three
classes) plus a seeded sample of the raw organic and plastic photos (there are no raw metal ones)."""
import os
import random
import sys

import cv2

ai, out = sys.argv[1], sys.argv[2]
raw = sys.argv[3]
os.makedirs(out, exist_ok=True)
n = 0
for c in ('metal', 'organic', 'plastic'):
    m = cv2.imread(f'{ai}/dataset/{c}_samples.jpg')
    th, tw = m.shape[0] // 2, m.shape[1] // 4
    for i in range(2):
        for j in range(4):
            cv2.imwrite(f'{out}/{c}_tile{i}{j}.jpg', m[i * th:(i + 1) * th, j * tw:(j + 1) * tw])
            n += 1
rng = random.Random(0)
for c in ('organic', 'plastic'):
    files = sorted(f for f in os.listdir(f'{raw}/{c}') if f.endswith('.jpg'))
    for f in rng.sample(files, 40):
        img = cv2.imread(f'{raw}/{c}/{f}')
        if img is None:
            continue
        s = 800 / max(img.shape[:2])
        if s < 1:
            img = cv2.resize(img, None, fx=s, fy=s, interpolation=cv2.INTER_AREA)
        cv2.imwrite(f'{out}/{c}_raw_{f}', img)
        n += 1
print(n, 'images in', out)