"""Static INT8 quantization of the waste model, calibrated on simulated screen frames.

Weights AND activations become 8-bit integers (4x smaller than float32, and the Pi 5's Cortex-A76
has integer dot-product instructions that ONNX Runtime uses). "Static" means the activation ranges
are measured once, here, on calibration frames that look like what the robot will see, instead of
being guessed per frame at run time. The detection head (model.23) is left in float: it turns
features into box coordinates and scores, where rounding errors hurt most.

python3 -I quantize_int8.py <in.onnx> <out.onnx> <eval_dir> <pkg_dir>
"""
import glob
import os
import sys

import cv2
import numpy as np
import onnx
from onnxruntime.quantization import (CalibrationDataReader, CalibrationMethod, QuantFormat,
                                      QuantType, quantize_static)
from onnxruntime.quantization.shape_inference import quant_pre_process

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from screen_sim import degrade  # noqa: E402

src, dst, eval_dir, pkg_dir = sys.argv[1:5]
sys.path.insert(0, pkg_dir)
from auracle_vision.waste_model import WasteModel  # noqa: E402

model = WasteModel(src)
rng = np.random.default_rng(7)  # different seed from the evaluation frames
files = sorted(glob.glob(f'{eval_dir}/*.jpg'))[::5]   # ~20 images x2 frames: histogram calibration is memory-hungry
blobs = []
for f in files:
    img = cv2.imread(f)
    for fr in (img, degrade(img, rng)):
        canvas = model.letterbox(fr)[0]
        blobs.append(cv2.dnn.blobFromImage(canvas, 1 / 255.0, swapRB=True))


class Reader(CalibrationDataReader):
    def __init__(self):
        self.it = iter(blobs)

    def get_next(self):
        b = next(self.it, None)
        return None if b is None else {model.inp: b}


pre = dst + '.pre.onnx'
quant_pre_process(src, pre)
head = [n.name for n in onnx.load(pre).graph.node if n.name.startswith('/model.23/')]
quantize_static(pre, dst, Reader(), quant_format=QuantFormat.QDQ, per_channel=True,
                activation_type=QuantType.QUInt8, weight_type=QuantType.QInt8,
                calibrate_method=CalibrationMethod.Percentile, nodes_to_exclude=head,
                extra_options={'CalibPercentile': 99.99})
os.remove(pre)
# keep the Ultralytics metadata (class names) that the node reads
m = onnx.load(dst)
for p in onnx.load(src).metadata_props:
    m.metadata_props.add(key=p.key, value=p.value)
onnx.save(m, dst)
print(f'{dst}: {os.path.getsize(dst) / 1e6:.1f} MB from {len(blobs)} calibration frames, '
      f'{len(head)} head nodes kept float')