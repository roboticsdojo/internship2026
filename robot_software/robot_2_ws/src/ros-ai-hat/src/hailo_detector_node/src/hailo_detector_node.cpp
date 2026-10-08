// hailo_detector_node.cpp
//
// ROS2 node: subscribes to a sensor_msgs/Image topic, runs object detection
// on a Hailo-8L NPU via HailoRT's C++ Async Infer API, publishes results
// as vision_msgs/Detection2DArray.
//
// Deliberately does NOT use TAPPAS or GStreamer: this talks to libhailort
// directly, the same way you'd use any other inference library from C++.
// Getting TAPPAS to work was too painful! I'll probably won't try that again :(
//
// ---------------------------------------------------------------------------
// IMPORTANT -- things you WILL need to adjust for your exact model/version:
// ---------------------------------------------------------------------------
// 1. HailoRT's C++ API has shifted slightly across releases (this targets the
//    Async Infer API present in the 4.x line, which should cover 4.23/4.24).
//    (I am using hailort v4.24.0)
//    If a method name here doesn't match, check /usr/include/hailo/hailort.hpp
//    on your system (that header is the ground truth for your installed
//    version, not this file.) ONLY THE PARANOID SURVIVE
//
// 2. decode_detections() below assumes the .hef has NMS baked in on-chip
//    (HAILO_FORMAT_ORDER_HAILO_NMS_BY_CLASS), which is how Hailo Model Zoo
//    ships its YOLOv8 .hef files by default. Run:
//        hailortcli parse-hef your_model.hef
//    and check the output stream's format order. If it's NOT NMS-by-class
//    (i.e. you get raw tensor output instead), you need a different decode
//    step (anchor decoding + your own NMS) The model that I have included in 
//    the models directory is tested and works
//
// 3. Input preprocessing (resize/letterbox/normalization/quantization)
//    assumes a single UINT8 RGB input, which is the common default for
//    Hailo-quantized models -- verify against parse-hef output for your model.
// ---------------------------------------------------------------------------

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <vision_msgs/msg/detection2_d_array.hpp>
#include <vision_msgs/msg/detection2_d.hpp>
#include <vision_msgs/msg/object_hypothesis_with_pose.hpp>
#include <cv_bridge/cv_bridge.hpp>
#include <opencv2/opencv.hpp>

#include <hailo/hailort.hpp>

#include <memory>
#include <vector>
#include <string>
#include <chrono>

using namespace hailort;
using std::placeholders::_1;

struct Detection {
  float xmin, ymin, xmax, ymax;  // normalized [0,1] image-relative coords
  float score;
  int class_id;
};

class HailoDetectorNode : public rclcpp::Node {
public:
  HailoDetectorNode() : Node("hailo_detector_node") {
    // ---- Parameters ----
    hef_path_ = this->declare_parameter<std::string>("hef_path", "yolov8s.hef");
    input_topic_ = this->declare_parameter<std::string>("input_topic", "/camera/image_raw");
    output_topic_ = this->declare_parameter<std::string>("output_topic", "/hailo/detections");
    score_threshold_ = this->declare_parameter<double>("score_threshold", 0.5);

    RCLCPP_INFO(this->get_logger(), "Loading HEF: %s", hef_path_.c_str());
    init_hailo();

    detection_pub_ = this->create_publisher<vision_msgs::msg::Detection2DArray>(
        output_topic_, rclcpp::SensorDataQoS());

    image_sub_ = this->create_subscription<sensor_msgs::msg::Image>(
        input_topic_, rclcpp::SensorDataQoS(),
        std::bind(&HailoDetectorNode::image_callback, this, _1));

    RCLCPP_INFO(this->get_logger(),
                "hailo_detector_node ready. Subscribed: %s  Publishing: %s",
                input_topic_.c_str(), output_topic_.c_str());
  }

private:
  // ---------------------------------------------------------------------
  // HailoRT setup: create device, load HEF, configure the model, allocate
  // bindings once at startup so the hot path (image_callback) does no
  // allocation
  // ---------------------------------------------------------------------
  void init_hailo() {
    auto vdevice_exp = VDevice::create();
    if (!vdevice_exp) {
      throw std::runtime_error("Failed to create HailoRT VDevice - is the device visible? "
                                "Check `hailortcli scan`.");
    }
    vdevice_ = vdevice_exp.release();

    auto infer_model_exp = vdevice_->create_infer_model(hef_path_);
    if (!infer_model_exp) {
      throw std::runtime_error("Failed to create InferModel from HEF: " + hef_path_);
    }
    infer_model_ = infer_model_exp.release();
    infer_model_->set_batch_size(1);

    auto configured_exp = infer_model_->configure();
    if (!configured_exp) {
      throw std::runtime_error("Failed to configure InferModel.");
    }
    configured_model_ = std::make_shared<ConfiguredInferModel>(configured_exp.release());

    // Cache input/output stream info: you'll see these names/shapes printed by `hailortcli parse-hef yolov8s.hef`.
    input_name_ = infer_model_->get_input_names()[0];
    auto in_shape = infer_model_->input(input_name_)->shape();
    input_h_ = in_shape.height;
    input_w_ = in_shape.width;
    input_c_ = in_shape.features;

    for (auto &name : infer_model_->get_output_names()) {
      output_names_.push_back(name);
    }

    RCLCPP_INFO(this->get_logger(), "Model input: %dx%dx%d, %zu output stream(s)",
                input_w_, input_h_, input_c_, output_names_.size());
  }

  // ---------------------------------------------------------------------
  // Preprocess: letterbox-resize the incoming frame to the model's input dimensions and convert to the expected layout. Adjust color conversion/quantization here
  // if your model expects something other than straight UINT8 RGB.
  // ---------------------------------------------------------------------
  cv::Mat preprocess(const cv::Mat &bgr_frame) {
    cv::Mat rgb;
    cv::cvtColor(bgr_frame, rgb, cv::COLOR_BGR2RGB);

    cv::Mat resized;
    cv::resize(rgb, resized, cv::Size(input_w_, input_h_), 0, 0, cv::INTER_LINEAR);
    // NOTE: this is a plain resize, not a letterbox (no aspect-ratio padding).If your model was trained/exported expecting letterboxing, replace this with a pad-to-aspect-ratio resize
    // and remember to undo the padding offset when mapping boxes back to the original frame in decode step.
    return resized;
  }

  // ---------------------------------------------------------------------
  // Run one inference synchronously and block until done. For max throughput you'd pipeline this (submit next frame's async job while parsing the previous result)
  // Good first issue?
  // ---------------------------------------------------------------------
  std::vector<Detection> run_inference(const cv::Mat &input_frame) {
    auto bindings_exp = configured_model_->create_bindings();
    if (!bindings_exp) {
      RCLCPP_WARN(this->get_logger(), "Failed to create bindings for this frame, skipping.");
      return {};
    }
    auto bindings = bindings_exp.release();

    // Input buffer
    bindings.input(input_name_)->set_buffer(
        MemoryView(const_cast<uint8_t *>(input_frame.data),
                   input_frame.total() * input_frame.elemSize()));

    // Output buffers: one per output stream, sized per stream's frame size
    std::vector<std::vector<uint8_t>> output_buffers(output_names_.size());
    for (size_t i = 0; i < output_names_.size(); ++i) {
      size_t frame_size = infer_model_->output(output_names_[i])->get_frame_size();
      output_buffers[i].resize(frame_size);
      bindings.output(output_names_[i])->set_buffer(
          MemoryView(output_buffers[i].data(), output_buffers[i].size()));
    }

    auto job_exp = configured_model_->run_async(bindings, [](const AsyncInferCompletionInfo &) {});
    if (!job_exp) {
      RCLCPP_WARN(this->get_logger(), "run_async failed for this frame.");
      return {};
    }
    auto job = job_exp.release();
    job.wait(std::chrono::milliseconds(1000));

    return decode_detections(output_buffers);
  }

  // ---------------------------------------------------------------------
  // Decode NMS-on-chip output into Detection structs.
  //
  // Hailo's HAILO_NMS_BY_CLASS output layout (verify against parse-hef!):
  //   For each class:
  //     [uint32 num_boxes]
  //     [num_boxes x (ymin, xmin, ymax, xmax, score) as float32, normalized]
  // ---------------------------------------------------------------------
  
  std::vector<Detection> decode_detections(const std::vector<std::vector<uint8_t>> &output_buffers) {
    std::vector<Detection> detections;
    if (output_buffers.empty()) return detections;

    const uint8_t *raw = output_buffers[0].data();
    size_t offset = 0;
    const size_t total_size = output_buffers[0].size();

    for (int class_id = 0; offset < total_size; ++class_id) {
      uint32_t num_boxes;
      std::memcpy(&num_boxes, raw + offset, sizeof(uint32_t));
      offset += sizeof(uint32_t);

      for (uint32_t i = 0; i < num_boxes && offset + 5 * sizeof(float) <= total_size; ++i) {
        float ymin, xmin, ymax, xmax, score;
        std::memcpy(&ymin, raw + offset, sizeof(float));  offset += sizeof(float);
        std::memcpy(&xmin, raw + offset, sizeof(float));  offset += sizeof(float);
        std::memcpy(&ymax, raw + offset, sizeof(float));  offset += sizeof(float);
        std::memcpy(&xmax, raw + offset, sizeof(float));  offset += sizeof(float);
        std::memcpy(&score, raw + offset, sizeof(float)); offset += sizeof(float);

        if (score >= score_threshold_) {
          detections.push_back({xmin, ymin, xmax, ymax, score, class_id});
        }
      }
    }
    return detections;
  }

  // ---------------------------------------------------------------------
  // ROS2 callback: image in -> Detection2DArray out
  // ---------------------------------------------------------------------
  void image_callback(const sensor_msgs::msg::Image::ConstSharedPtr &msg) {
    cv_bridge::CvImagePtr cv_ptr;
    try {
      cv_ptr = cv_bridge::toCvCopy(msg, "bgr8");
    } catch (const cv_bridge::Exception &e) {
      RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
      return;
    }

    cv::Mat model_input = preprocess(cv_ptr->image);
    std::vector<Detection> detections = run_inference(model_input);

    vision_msgs::msg::Detection2DArray out_msg;
    out_msg.header = msg->header;

    const int img_w = cv_ptr->image.cols;
    const int img_h = cv_ptr->image.rows;

    for (const auto &det : detections) {
      vision_msgs::msg::Detection2D d;
      double cx = (det.xmin + det.xmax) / 2.0 * img_w;
      double cy = (det.ymin + det.ymax) / 2.0 * img_h;
      double w = (det.xmax - det.xmin) * img_w;
      double h = (det.ymax - det.ymin) * img_h;

      d.bbox.center.position.x = cx;
      d.bbox.center.position.y = cy;
      d.bbox.size_x = w;
      d.bbox.size_y = h;

      vision_msgs::msg::ObjectHypothesisWithPose hyp;
      hyp.hypothesis.class_id = std::to_string(det.class_id);
      hyp.hypothesis.score = det.score;
      d.results.push_back(hyp);

      out_msg.detections.push_back(d);
    }

    detection_pub_->publish(out_msg);
  }

  // ---- Members ----
  std::string hef_path_, input_topic_, output_topic_;
  double score_threshold_;

  std::unique_ptr<VDevice> vdevice_;
  std::shared_ptr<InferModel> infer_model_;
  std::shared_ptr<ConfiguredInferModel> configured_model_;

  std::string input_name_;
  std::vector<std::string> output_names_;
  int input_w_ = 0, input_h_ = 0, input_c_ = 0;

  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_sub_;
  rclcpp::Publisher<vision_msgs::msg::Detection2DArray>::SharedPtr detection_pub_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  try {
    auto node = std::make_shared<HailoDetectorNode>();
    rclcpp::spin(node);
  } catch (const std::exception &e) {
    RCLCPP_FATAL(rclcpp::get_logger("hailo_detector_node"), "Fatal error: %s", e.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
