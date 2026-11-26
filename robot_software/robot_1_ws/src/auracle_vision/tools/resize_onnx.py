#!/usr/bin/env python3
"""Change the input size of an exported Ultralytics YOLO26 detect ONNX (static shapes) without the .pt.

Why this works: the network is convolutional, so its weights don't care about image size. Only a few
constants baked in at export time do:
  * the attention blocks reshape the stride-32 feature map with fixed numbers (20x20 = 400 at 640 px)
  * the head has fixed anchor centres (8400 of them at 640 px) and the matching stride per anchor
This script rebuilds those constants for the new size and re-runs ONNX shape inference.

Usage: python3 resize_onnx.py in.onnx out.onnx 320
"""
import sys

import numpy as np
import onnx
from onnx import numpy_helper, shape_inference

STRIDES = (8, 16, 32)


def anchors(size):
    pts, strides = [], []
    for s in STRIDES:
        n = size // s
        sx = np.arange(n, dtype=np.float32) + 0.5
        sy = np.arange(n, dtype=np.float32) + 0.5
        yy, xx = np.meshgrid(sy, sx, indexing='ij')
        pts.append(np.stack([xx.ravel(), yy.ravel()], 0))
        strides.append(np.full(n * n, s, np.float32))
    return np.concatenate(pts, 1)[None], np.concatenate(strides)[None]


def main(src, dst, size):
    assert size % 32 == 0, 'size must be a multiple of 32'
    m = onnx.load(src)
    g = m.graph
    old = g.input[0].type.tensor_type.shape.dim[2].dim_value
    o32, n32 = old // 32, size // 32
    a_old, s_old = anchors(old)
    a_new, s_new = anchors(size)
    changed = 0
    for node in g.node:
        if node.op_type != 'Constant':
            continue
        t = node.attribute[0].t
        a = numpy_helper.to_array(t)
        new = None
        if a.shape == a_old.shape and np.allclose(a, a_old):
            new = a_new
        elif a.shape == s_old.shape and np.allclose(a, s_old):
            new = s_new
        elif a.dtype == np.int64 and a.ndim == 1 and a.size == 4:
            v = a.tolist()
            if v[1:] == [2, 128, o32 * o32] or v[2:] == [128, o32 * o32]:
                new = np.array(v[:3] + [n32 * n32], np.int64)
            elif v[1:] == [128, o32, o32]:
                new = np.array([v[0], 128, n32, n32], np.int64)
        if new is not None:
            node.attribute[0].t.CopyFrom(numpy_helper.from_array(new.astype(a.dtype), t.name))
            changed += 1
    if changed == 0:
        sys.exit('no size-dependent constants found: is this a YOLO26 detect export?')
    g.input[0].type.tensor_type.shape.dim[2].dim_value = size
    g.input[0].type.tensor_type.shape.dim[3].dim_value = size
    g.output[0].type.tensor_type.shape.dim[2].dim_value = a_new.shape[2]
    del g.value_info[:]
    for p in m.metadata_props:
        if p.key == 'imgsz':
            p.value = f'[{size}, {size}]'
    m = shape_inference.infer_shapes(m, strict_mode=True)
    onnx.checker.check_model(m)
    onnx.save(m, dst)
    print(f'{src} ({old}px) -> {dst} ({size}px): {changed} constants rebuilt, '
          f'{a_new.shape[2]} anchors')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], int(sys.argv[3]))