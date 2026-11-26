"""waste_detector: camera frames in, ONE waste class out, only when the behaviour tree asks.

Talks to auracle_bt (bt_runner, node WaitForDetection) over two topics, relative to the namespace:
  vision/enable    std_msgs/Bool    in   true = "I'm waiting for a class", false = "got it, stop"
  vision/decision  std_msgs/String  out  "metal" | "organic" | "plastic", published once per vote win
Debug / tuning:
  vision/detections vision_msgs/Detection2DArray  every processed frame (only while enabled)
  vision/status     std_msgs/String               vote tally once a second while enabled

Why gated: inference costs CPU that SLAM, AMCL and the controllers need while driving. Disabled, the
node only keeps the newest frame pointer, nothing else runs.
Why a timer instead of the image callback: the camera may send 30 fps. We infer at `rate_hz` on the
newest frame and drop the rest, so frames never queue up and the answer is never stale.
"""
import os
import time

import rclpy
from cv_bridge import CvBridge
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy
from sensor_msgs.msg import Image
from std_msgs.msg import Bool, String
from vision_msgs.msg import Detection2D, Detection2DArray, ObjectHypothesisWithPose
from ament_index_python.packages import get_package_share_directory
import cv2

from auracle_vision.waste_model import Voter, WasteModel


class WasteDetector(Node):
    def __init__(self):
        super().__init__('waste_detector')
        models = os.path.join(get_package_share_directory('auracle_vision'), 'models')
        p = self.declare_parameter
        p('model', 'waste_yolo26n_320.onnx')   # file name in share/models, or a full path
        p('threads', 2)            # onnxruntime threads (Pi 5 has 4 cores; leave some for Nav2)
        p('conf', 0.25)            # per-frame score floor; the vote does the real filtering
        p('iou', 0.45)
        p('clahe', False)          # local-contrast boost for washed-out screens (+~50% time)
        p('roi', [0.0, 0.0, 1.0, 1.0])   # crop x0,y0,x1,y1 as fractions of the frame (screen area)
        p('rate_hz', 5.0)          # inference rate while enabled
        p('window', 10)            # vote over the last N frames ...
        p('min_votes', 6)          # ... a class needs this many of them ...
        p('min_score', 0.35)       # ... with at least this average score
        p('always_on', False)      # true: run without the BT asking (bench testing)
        p('save_dir', '')          # non-empty: save every processed frame there (dataset!)
        p('image_topic', 'camera/image_raw')

        g = lambda n: self.get_parameter(n).value  # noqa: E731
        path = g('model') if os.path.isabs(g('model')) else os.path.join(models, g('model'))
        self.model = WasteModel(path, threads=g('threads'), conf=g('conf'), iou=g('iou'),
                                clahe=g('clahe'))
        self.voter = Voter(g('window'), g('min_votes'), g('min_score'))
        self.roi = [float(v) for v in g('roi')]
        self.save_dir = g('save_dir')
        if self.save_dir:
            os.makedirs(self.save_dir, exist_ok=True)
        self.enabled = bool(g('always_on'))
        self.always_on = self.enabled
        self.frame = None          # newest Image msg (not yet converted: conversion costs CPU)
        self.frame_id = 0
        self.done_id = -1
        self.bridge = CvBridge()

        # camera: keep only the newest frame, never wait for a lost one
        cam_qos = QoSProfile(depth=1, reliability=ReliabilityPolicy.BEST_EFFORT,
                             history=HistoryPolicy.KEEP_LAST)
        self.create_subscription(Image, g('image_topic'), self.on_image, cam_qos)
        self.create_subscription(Bool, 'vision/enable', self.on_enable, 10)
        self.pub_decision = self.create_publisher(String, 'vision/decision', 10)
        self.pub_det = self.create_publisher(Detection2DArray, 'vision/detections', 1)
        self.pub_status = self.create_publisher(String, 'vision/status', 1)
        self.create_timer(1.0 / g('rate_hz'), self.step)
        self.create_timer(1.0, self.status)
        self.get_logger().info(
            f'{os.path.basename(path)} ({self.model.size}px, {g("threads")} threads), '
            f'vote {g("min_votes")}/{g("window")} @ {g("rate_hz")} Hz, '
            f'{"always on" if self.enabled else "waiting for vision/enable"}')

    def on_image(self, msg):
        self.frame = msg
        self.frame_id += 1

    def on_enable(self, msg):
        if msg.data and not self.enabled:
            self.voter.reset()     # votes from before the request are stale: start clean
            self.get_logger().info('enabled: looking at the screen')
        elif not msg.data and self.enabled and not self.always_on:
            self.get_logger().info('disabled')
        self.enabled = bool(msg.data) or self.always_on

    def step(self):
        if not self.enabled or self.frame is None or self.frame_id == self.done_id:
            return
        self.done_id = self.frame_id
        msg = self.frame
        img = self.bridge.imgmsg_to_cv2(msg, 'bgr8')
        h, w = img.shape[:2]
        x0, y0, x1, y1 = self.roi
        ox, oy = int(x0 * w), int(y0 * h)
        img = img[oy:int(y1 * h), ox:int(x1 * w)]
        t0 = time.perf_counter()
        dets = self.model.detect(img)
        ms = (time.perf_counter() - t0) * 1000
        if self.save_dir:
            cv2.imwrite(os.path.join(self.save_dir, f'{time.time():.3f}.jpg'), img)
        self.publish_detections(msg, dets, ox, oy)
        best = (dets[0][0], dets[0][1]) if dets else None
        decision = self.voter.add(best)
        self.get_logger().debug(f'{best} {ms:.0f} ms')
        if decision:
            label, n, score = decision
            self.get_logger().info(f'DECISION {label} ({n}/{self.voter.window} frames, '
                                   f'mean score {score:.2f}, last frame {ms:.0f} ms)')
            self.pub_decision.publish(String(data=label))
            self.voter.reset()     # one decision per vote; the BT disables us when it has it

    def publish_detections(self, msg, dets, ox, oy):
        out = Detection2DArray()
        out.header = msg.header
        for name, score, (x, y, bw, bh) in dets:
            d = Detection2D()
            d.header = msg.header
            d.bbox.center.position.x = ox + x + bw / 2
            d.bbox.center.position.y = oy + y + bh / 2
            d.bbox.size_x = bw
            d.bbox.size_y = bh
            hyp = ObjectHypothesisWithPose()
            hyp.hypothesis.class_id = name
            hyp.hypothesis.score = score
            d.results.append(hyp)
            out.detections.append(d)
        self.pub_det.publish(out)

    def status(self):
        if self.enabled:
            tally = dict(self.voter.tally())
            self.pub_status.publish(String(data=f'{len(self.voter.frames)} frames {tally}'))


def main():
    rclpy.init()
    node = WasteDetector()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.try_shutdown()


if __name__ == '__main__':
    main()