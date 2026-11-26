"""Waste model core: no ROS in here, so it can be tested on a laptop (tools/eval_screen.py).

Two pieces:
  WasteModel  one camera frame in -> list of detections out (YOLO26n ONNX, run with onnxruntime)
  Voter       many frames' best guesses in -> one stable answer out (or "not sure yet")

Why a voter: one frame of a cheap camera pointed at a cheap screen is noisy (moire, glare, blur,
refresh flicker). A single wrong frame must not send the robot to the wrong region. Asking for the
same answer in most of the last N frames filters out the odd bad frame, at the cost of N frames of
time. It only helps if the model is right MORE often than it is wrong on a frame: voting a model
that is mostly wrong just makes it confidently wrong. So the model must be good on screen frames
first (fine-tune on frames saved with save_dir), and the vote makes it steady.
"""
from collections import Counter, deque
import ast

import cv2
import numpy as np
import onnxruntime as ort

PAD = 114  # grey used by Ultralytics for letterbox padding: the model was trained on it


class WasteModel:
    def __init__(self, model_path, threads=2, conf=0.25, iou=0.45, clahe=False):
        opts = ort.SessionOptions()
        # Threads: the Pi 5 has 4 cores and Nav2, AMCL and ros2_control need some of them. 2 threads
        # leaves room; 4 is faster for this node but can make the control loop late.
        opts.intra_op_num_threads = int(threads)
        opts.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        self.sess = ort.InferenceSession(model_path, opts, providers=['CPUExecutionProvider'])
        inp = self.sess.get_inputs()[0]
        self.inp = inp.name
        self.size = int(inp.shape[2])  # 640, 416 or 320: read from the file, not hard-coded
        meta = self.sess.get_modelmeta().custom_metadata_map
        names = ast.literal_eval(meta['names']) if 'names' in meta else {0: 'metal', 1: 'organic', 2: 'plastic'}
        self.names = [names[i] for i in sorted(names)]
        self.conf = conf
        self.iou = iou
        self.clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8)) if clahe else None

    def letterbox(self, img):
        """Scale to fit size x size, keep the aspect ratio, pad with grey."""
        s = self.size
        h, w = img.shape[:2]
        r = min(s / h, s / w)
        nh, nw = round(h * r), round(w * r)
        # INTER_AREA averages every source pixel when shrinking. INTER_LINEAR (cv2's default) only
        # looks at 4 neighbours and skips the rest, which aliases fine patterns, and a photographed
        # screen is full of them (the pixel grid -> moire). Averaging is a low-pass filter first.
        interp = cv2.INTER_AREA if r < 1 else cv2.INTER_LINEAR
        resized = cv2.resize(img, (nw, nh), interpolation=interp)
        canvas = np.full((s, s, 3), PAD, np.uint8)
        top, left = (s - nh) // 2, (s - nw) // 2
        canvas[top:top + nh, left:left + nw] = resized
        return canvas, r, left, top

    def enhance(self, img):
        """Optional: stretch local contrast on the lightness channel only (colours untouched)."""
        if self.clahe is None:
            return img
        lab = cv2.cvtColor(img, cv2.COLOR_BGR2LAB)
        lab[:, :, 0] = self.clahe.apply(lab[:, :, 0])
        return cv2.cvtColor(lab, cv2.COLOR_LAB2BGR)

    def detect(self, bgr):
        """BGR frame -> [(class_name, score, (x, y, w, h) in frame pixels)], best first."""
        canvas, r, left, top = self.letterbox(self.enhance(bgr))
        # blobFromImage does /255, BGR->RGB and HWC->NCHW in one C++ pass
        blob = cv2.dnn.blobFromImage(canvas, 1 / 255.0, swapRB=True)
        pred = self.sess.run(None, {self.inp: blob})[0][0].T   # (anchors, 4 + classes)
        scores = pred[:, 4:]
        cls = scores.argmax(1)
        conf = scores[np.arange(len(cls)), cls]
        keep = conf > self.conf
        if not keep.any():
            return []
        pred, cls, conf = pred[keep], cls[keep], conf[keep]
        cx, cy, w, h = pred[:, 0], pred[:, 1], pred[:, 2], pred[:, 3]
        x = (cx - w / 2 - left) / r
        y = (cy - h / 2 - top) / r
        boxes = np.stack([x, y, w / r, h / r], 1)
        idx = cv2.dnn.NMSBoxes(boxes.tolist(), conf.tolist(), self.conf, self.iou)
        out = [(self.names[int(cls[i])], float(conf[i]), tuple(float(v) for v in boxes[i]))
               for i in np.array(idx).flatten()]
        return sorted(out, key=lambda d: -d[1])


class Voter:
    """Rolling vote over the last `window` frames.

    Each frame gives its best (class, score) or None (nothing found). A class wins when it is the
    best guess in at least `min_votes` of the last `window` frames AND its average score in those
    frames is at least `min_score`. Until then the answer is None ("not sure yet").
    """

    def __init__(self, window=10, min_votes=6, min_score=0.35):
        self.window = window
        self.min_votes = min_votes
        self.min_score = min_score
        self.frames = deque(maxlen=window)

    def reset(self):
        self.frames.clear()

    def add(self, best):
        self.frames.append(best)
        return self.decision()

    def tally(self):
        votes = Counter(b[0] for b in self.frames if b is not None)
        return votes

    def decision(self):
        votes = self.tally()
        if not votes:
            return None
        label, n = votes.most_common(1)[0]
        if n < self.min_votes:
            return None
        mean = np.mean([b[1] for b in self.frames if b is not None and b[0] == label])
        if mean < self.min_score:
            return None
        return label, n, float(mean)