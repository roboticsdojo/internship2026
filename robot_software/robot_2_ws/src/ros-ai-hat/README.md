# ROS AI HAT+

A ROS2 workspace containing packages that perform inference on the AI HAT+ (Compatible with Raspberry Pi 5)
Falls back to CPU inference if the AI HAT+ is not present.

- You'll have to manually set this.

## Setup

```bash
sudo apt install python3-opencv python3-numpy ros-jazzy-cv-bridge ros-jazzy-vision-msgs ros-jazzy-image-transport
pip install onnxruntime --break-system-packages
```

`onnxruntime` isn't a rosdep-resolvable key, hence the separate `pip install`
rather than `rosdep install` picking it up automatically.

Build Packages:

```bash
colcon build
source install/setup.bash
```

## CPU-native Inference

```bash
ros2 launch cpu_yolo_detector cpu_detector.launch.py \
  model_path:=/path/to/yolov8n.onnx \
  input_topic:=/camera/image_raw
```

## Hailo Inference

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
