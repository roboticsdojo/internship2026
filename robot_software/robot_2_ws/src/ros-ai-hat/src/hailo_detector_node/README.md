# hailo_yolo_detector

Minimal ROS2 node that runs object detection on a Hailo-8L NPU using
HailoRT's C++ Async Infer API directly — **no TAPPAS, no GStreamer, no
Python bindings**. This is intentional: it reduces the dependency chain
down to just `libhailort` + a `.hef` model.

## Prerequisites

- ROS2 Jazzy installed, sourced.
- HailoRT runtime + driver installed and version-matched
  (`hailortcli fw-control identify` should succeed with no version errors).
- HailoRT headers available (should come with the `hailort` apt package;
  confirm `/usr/include/hailo/hailort.hpp` exists).
- A compiled `.hef` model (e.g. `yolov8n.hef` the one used here was sourced from Robotics Dojo).
- ROS2 packages: `ros-jazzy-cv-bridge`, `ros-jazzy-vision-msgs`,
  `ros-jazzy-image-transport`.

## Run

```bash
ros2 launch hailo_yolo_detector detector.launch.py \
  hef_path:=/path/to/yolov8n.hef \
  input_topic:=/camera/image_raw
```

Detections publish on `/hailo/detections` as `vision_msgs/Detection2DArray`.
Check them with:

```bash
ros2 topic hz /hailo/detections    # your real end-to-end FPS
ros2 topic echo /hailo/detections
```

## Before this will work correctly for YOUR model

This is a working skeleton, not a versatile solution:object 
detection postprocessing is model-specific. Two things to verify
against your actual `.hef`:

```bash
hailortcli parse-hef /path/to/yolov8n.hef
```

1. **Input shape/format** — confirm width/height/format matches what
   `preprocess()` produces (this code assumes UINT8 RGB, plain resize,
   no letterbox padding).
2. **Output format** — `decode_detections()` assumes on-chip NMS with
   `HAILO_NMS_BY_CLASS`-style layout (per-class count + boxes), which is
   how Hailo Model Zoo ships YOLOv8 `.hef` files by default. If your
   model's output format differs, the byte layout in `decode_detections()`
   needs to change accordingly.
