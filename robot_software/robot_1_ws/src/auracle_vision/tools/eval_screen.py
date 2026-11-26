"""Compare pipeline choices on simulated screen photos. Same frames for every config (fixed seed).

python3 -I eval_screen.py <eval_dir> <models_dir> <pkg_dir> <out.json>
"""
import glob
import json
import os
import sys
import time

import cv2
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from screen_sim import degrade  # noqa: E402

eval_dir, models_dir, pkg_dir, out = sys.argv[1:5]
sys.path.insert(0, pkg_dir)
from auracle_vision.waste_model import WasteModel  # noqa: E402

FRAMES = 10
files = sorted(glob.glob(f'{eval_dir}/*.jpg'))
truth = [os.path.basename(f).split('_')[0] for f in files]
rng = np.random.default_rng(42)
clean = [cv2.imread(f) for f in files]
frames = [[degrade(img, rng) for _ in range(FRAMES)] for img in clean]
print(len(files), 'images x', FRAMES, 'frames', flush=True)


class LinearModel(WasteModel):
    """The old node's resize (cv2 default INTER_LINEAR) for comparison."""

    def letterbox(self, img):
        s = self.size
        h, w = img.shape[:2]
        r = min(s / h, s / w)
        nh, nw = round(h * r), round(w * r)
        resized = cv2.resize(img, (nw, nh))
        canvas = np.full((s, s, 3), 114, np.uint8)
        top, left = (s - nh) // 2, (s - nw) // 2
        canvas[top:top + nh, left:left + nw] = resized
        return canvas, r, left, top


configs = [
    ('640 linear', LinearModel, 'waste_yolo26n.onnx', {}),
    ('640 area', WasteModel, 'waste_yolo26n.onnx', {}),
    ('640 area clahe', WasteModel, 'waste_yolo26n.onnx', {'clahe': True}),
    ('416 area', WasteModel, 'waste_yolo26n_416.onnx', {}),
    ('320 linear', LinearModel, 'waste_yolo26n_320.onnx', {}),
    ('320 area', WasteModel, 'waste_yolo26n_320.onnx', {}),
    ('320 area clahe', WasteModel, 'waste_yolo26n_320.onnx', {'clahe': True}),
]
extra = sys.argv[5:]  # more model files, e.g. int8, run with "area"
for f in extra:
    configs.append((os.path.basename(f), WasteModel, f, {}))

results = {'truth': truth, 'configs': {}}
for name, cls, mf, kw in configs:
    m = cls(os.path.join(models_dir, mf), threads=2, conf=0.10, **kw)
    m.detect(clean[0])  # warm-up
    t0 = time.perf_counter()
    clean_best = []
    for img in clean:
        d = m.detect(img)
        clean_best.append([d[0][0], d[0][1]] if d else None)
    sim_best = []
    n = 0
    for fl in frames:
        row = []
        for fr in fl:
            d = m.detect(fr)
            n += 1
            row.append([d[0][0], d[0][1]] if d else None)
        sim_best.append(row)
    dt = (time.perf_counter() - t0) / (n + len(clean)) * 1000
    results['configs'][name] = {'ms': dt, 'clean': clean_best, 'sim': sim_best}
    print(f'{name:16s} {dt:6.1f} ms/frame', flush=True)
json.dump(results, open(out, 'w'))