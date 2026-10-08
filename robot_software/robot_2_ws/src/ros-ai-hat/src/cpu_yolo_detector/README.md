# cpu_yolo_detector

CPU-only YOLOv8 object detection ROS2 node using ONNX Runtime.

## Run

```bash
ros2 launch cpu_yolo_detector cpu_detector.launch.py \
  model_path:=/path/to/yolov8s.onnx \
  input_topic:=/camera/image_raw
```
