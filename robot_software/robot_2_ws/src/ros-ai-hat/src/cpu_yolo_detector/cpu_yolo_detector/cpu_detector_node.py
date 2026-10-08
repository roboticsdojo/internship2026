#!/usr/bin/env python3
"""
cpu_detector_node.py

CPU-only YOLOv8 object detection via ONNX Runtime. Subscribes to a sensor_msgs/Image topic, runs inference on CPU, 
publishes vision_msgs/Detection2DArray

---------------------------------------------------------------------------
Assumptions (match these to how you exported your .onnx model):
---------------------------------------------------------------------------
1. Standard Ultralytics YOLOv8 ONNX export (no NMS baked into the graph),
   output shape (1, 4 + num_classes, num_anchors) e.g. (1, 84, 8400) for
   80-class COCO at 640 input. If you exported with `nms=True`, the output
   format differs and postprocess() below will need adjusting.
2. Letterbox preprocessing (aspect-ratio-preserving resize + padding),
   matching what Ultralytics uses internally
---------------------------------------------------------------------------

Usage:
  ros2 run cpu_yolo_detector cpu_detector_node --ros-args \
    -p model_path:=/path/to/yolov8n.onnx \
    -p input_topic:=/camera/image_raw \
    -p output_topic:=/cpu/detections \
    -p score_threshold:=0.5 \
    -p num_threads:=4
"""

import time

import cv2
import numpy as np
import onnxruntime as ort
import rclpy
from cv_bridge import CvBridge
from rclpy.node import Node
from sensor_msgs.msg import Image
from vision_msgs.msg import Detection2D, Detection2DArray, ObjectHypothesisWithPose


def letterbox(img, new_size=640, color=(114, 114, 114)):
    """Resize+pad image to a square new_size x new_size, preserving aspect ratio.
    Returns the padded image, the scale factor applied, and the (left, top) padding."""
    h, w = img.shape[:2]
    r = min(new_size / h, new_size / w)
    new_unpad_w, new_unpad_h = int(round(w * r)), int(round(h * r))

    resized = cv2.resize(img, (new_unpad_w, new_unpad_h), interpolation=cv2.INTER_LINEAR)

    dw, dh = new_size - new_unpad_w, new_size - new_unpad_h
    left, right = dw // 2, dw - dw // 2
    top, bottom = dh // 2, dh - dh // 2

    padded = cv2.copyMakeBorder(resized, top, bottom, left, right,
                                 cv2.BORDER_CONSTANT, value=color)
    return padded, r, (left, top)


class CpuDetectorNode(Node):
    def __init__(self):
        super().__init__('cpu_detector_node')

        self.declare_parameter('model_path', '')
        self.declare_parameter('input_topic', '/camera/image_raw')
        self.declare_parameter('output_topic', '/cpu/detections')
        self.declare_parameter('imgsz', 640)
        self.declare_parameter('score_threshold', 0.5)
        self.declare_parameter('iou_threshold', 0.45)
        self.declare_parameter('num_threads', 4)

        model_path = self.get_parameter('model_path').value
        self.input_topic = self.get_parameter('input_topic').value
        self.output_topic = self.get_parameter('output_topic').value
        self.imgsz = self.get_parameter('imgsz').value
        self.score_threshold = self.get_parameter('score_threshold').value
        self.iou_threshold = self.get_parameter('iou_threshold').value
        num_threads = self.get_parameter('num_threads').value

        if not model_path:
            raise ValueError("model_path parameter is required, e.g. -p model_path:=/path/to/yolov8n.onnx")

        self.get_logger().info(f"Loading ONNX model: {model_path} (intra_op_num_threads={num_threads})")
        sess_options = ort.SessionOptions()
        sess_options.intra_op_num_threads = num_threads
        self.session = ort.InferenceSession(
            model_path, sess_options=sess_options, providers=['CPUExecutionProvider'])

        self.input_name = self.session.get_inputs()[0].name
        out_shape = self.session.get_outputs()[0].shape
        self.get_logger().info(f"Model input: {self.input_name}, output shape: {out_shape}")

        self.bridge = CvBridge()
        self.detection_pub = self.create_publisher(
            Detection2DArray, self.output_topic, rclpy.qos.qos_profile_sensor_data)
        self.image_sub = self.create_subscription(
            Image, self.input_topic, self.image_callback, rclpy.qos.qos_profile_sensor_data)

        self.get_logger().info(
            f"cpu_detector_node ready. Subscribed: {self.input_topic}  Publishing: {self.output_topic}")

    def preprocess(self, bgr_frame):
        padded, scale, pad = letterbox(bgr_frame, self.imgsz)
        rgb = cv2.cvtColor(padded, cv2.COLOR_BGR2RGB)
        blob = rgb.astype(np.float32) / 255.0
        blob = blob.transpose(2, 0, 1)[np.newaxis, ...]  # HWC -> NCHW
        return np.ascontiguousarray(blob), scale, pad

    def postprocess(self, output, scale, pad, orig_w, orig_h):
        # output: (1, 4 + num_classes, num_anchors) -> (num_anchors, 4 + num_classes)
        preds = output[0].transpose(1, 0)

        boxes_xywh = preds[:, :4]
        class_scores = preds[:, 4:]
        class_ids = np.argmax(class_scores, axis=1)
        confidences = class_scores[np.arange(class_scores.shape[0]), class_ids]

        keep = confidences > self.score_threshold
        if not np.any(keep):
            return []

        boxes_xywh = boxes_xywh[keep]
        confidences = confidences[keep]
        class_ids = class_ids[keep]

        # cx,cy,w,h (letterboxed pixel space) -> x,y,w,h top-left, for cv2.dnn.NMSBoxes
        cx, cy, w, h = boxes_xywh[:, 0], boxes_xywh[:, 1], boxes_xywh[:, 2], boxes_xywh[:, 3]
        x = cx - w / 2
        y = cy - h / 2
        nms_boxes = np.stack([x, y, w, h], axis=1).tolist()

        indices = cv2.dnn.NMSBoxes(nms_boxes, confidences.tolist(),
                                    self.score_threshold, self.iou_threshold)
        if len(indices) == 0:
            return []
        indices = np.array(indices).flatten()

        left, top = pad
        detections = []
        for i in indices:
            bx, by, bw, bh = nms_boxes[i]
            # undo letterbox: remove padding, then unscale to original image size
            x1 = (bx - left) / scale
            y1 = (by - top) / scale
            x2 = (bx + bw - left) / scale
            y2 = (by + bh - top) / scale
            x1, x2 = np.clip([x1, x2], 0, orig_w)
            y1, y2 = np.clip([y1, y2], 0, orig_h)
            detections.append((x1, y1, x2, y2, float(confidences[i]), int(class_ids[i])))
        return detections

    def image_callback(self, msg: Image):
        try:
            cv_frame = self.bridge.imgmsg_to_cv2(msg, desired_encoding='bgr8')
        except Exception as e:
            self.get_logger().error(f"cv_bridge conversion failed: {e}")
            return

        orig_h, orig_w = cv_frame.shape[:2]
        blob, scale, pad = self.preprocess(cv_frame)

        output = self.session.run(None, {self.input_name: blob})[0]
        detections = self.postprocess(output, scale, pad, orig_w, orig_h)

        out_msg = Detection2DArray()
        out_msg.header = msg.header  # preserved, same convention as hailo_yolo_detector

        for (x1, y1, x2, y2, score, class_id) in detections:
            d = Detection2D()
            d.bbox.center.position.x = (x1 + x2) / 2.0
            d.bbox.center.position.y = (y1 + y2) / 2.0
            d.bbox.size_x = x2 - x1
            d.bbox.size_y = y2 - y1

            hyp = ObjectHypothesisWithPose()
            hyp.hypothesis.class_id = str(class_id)
            hyp.hypothesis.score = score
            d.results.append(hyp)

            out_msg.detections.append(d)

        self.detection_pub.publish(out_msg)


def main(args=None):
    rclpy.init(args=args)
    node = CpuDetectorNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
