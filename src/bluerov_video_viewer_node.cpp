#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/image_encodings.hpp>

using namespace std::chrono_literals;

class BlueROVVideoViewer : public rclcpp::Node
{
public:
  BlueROVVideoViewer()
  : Node("bluerov_video_viewer")
  {
    const std::string ip_address = declare_parameter<std::string>("ip_address", "0.0.0.0");
    const int port = declare_parameter<int>("port", 5600);
    pipeline_ = declare_parameter<std::string>("pipeline", "");
    window_name_ = declare_parameter<std::string>("window_name", "BlueROV camera");
    frame_id_ = declare_parameter<std::string>("frame_id", "bluerov_camera");
    publish_ros_topic_ = declare_parameter<bool>("publish_ros_topic", true);
    show_window_ = declare_parameter<bool>("show_window", true);

    if (ip_address.empty()) {
      throw std::invalid_argument("Parameter 'ip_address' must not be empty");
    }
    if (port < 1 || port > 65535) {
      throw std::invalid_argument("Parameter 'port' must be between 1 and 65535");
    }

    if (pipeline_.empty()) {
      pipeline_ =
        "udpsrc address=" + ip_address + " port=" + std::to_string(port) +
        " caps=\"application/x-rtp,media=video,clock-rate=90000,encoding-name=H264,payload=96\" ! "
        "rtpjitterbuffer latency=100 drop-on-latency=true ! "
        "rtph264depay ! avdec_h264 ! videoconvert ! "
        "video/x-raw,format=BGR ! appsink name=appsink max-buffers=1 drop=true sync=false";
    }

    if (publish_ros_topic_) {
      publisher_ = create_publisher<sensor_msgs::msg::Image>(
        "image_raw", rclcpp::SensorDataQoS());
    }

    RCLCPP_INFO(get_logger(), "Opening BlueROV video stream with pipeline: %s", pipeline_.c_str());
    if (!capture_.open(pipeline_, cv::CAP_GSTREAMER)) {
      throw std::runtime_error(
              "Could not open the GStreamer stream. Check the pipeline, plugins, firewall, and UDP port.");
    }

    if (show_window_) {
      cv::namedWindow(window_name_, cv::WINDOW_NORMAL);
    }

    // A zero-period wall timer lets GStreamer determine the frame rate.
    timer_ = create_wall_timer(1ms, std::bind(&BlueROVVideoViewer::read_frame, this));
  }

  ~BlueROVVideoViewer() override
  {
    capture_.release();
    if (show_window_) {
      cv::destroyWindow(window_name_);
    }
  }

private:
  void read_frame()
  {
    cv::Mat frame;
    if (!capture_.read(frame) || frame.empty()) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 5000,
        "No video frame received. Waiting for H.264 RTP packets on the configured UDP port.");
      return;
    }

    if (show_window_) {
      cv::imshow(window_name_, frame);
      const int key = cv::waitKey(1);
      if (key == 27 || key == 'q' || key == 'Q') {
        RCLCPP_INFO(get_logger(), "Viewer closed by keyboard request");
        rclcpp::shutdown();
        return;
      }
    }

    if (!publish_ros_topic_) {
      return;
    }

    auto message = std::make_unique<sensor_msgs::msg::Image>();
    message->header.stamp = now();
    message->header.frame_id = frame_id_;
    message->height = static_cast<uint32_t>(frame.rows);
    message->width = static_cast<uint32_t>(frame.cols);
    message->encoding = sensor_msgs::image_encodings::BGR8;
    message->is_bigendian = false;
    message->step = static_cast<sensor_msgs::msg::Image::_step_type>(frame.cols * frame.elemSize());
    message->data.resize(static_cast<std::size_t>(message->step) * message->height);

    if (frame.isContinuous() && frame.step == message->step) {
      std::memcpy(message->data.data(), frame.data, message->data.size());
    } else {
      for (int row = 0; row < frame.rows; ++row) {
        std::memcpy(
          message->data.data() + static_cast<std::size_t>(row) * message->step,
          frame.ptr(row), message->step);
      }
    }
    publisher_->publish(std::move(message));
  }

  std::string pipeline_;
  std::string window_name_;
  std::string frame_id_;
  bool publish_ros_topic_{};
  bool show_window_{};
  cv::VideoCapture capture_;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  try {
    rclcpp::spin(std::make_shared<BlueROVVideoViewer>());
  } catch (const std::exception & error) {
    RCLCPP_FATAL(rclcpp::get_logger("bluerov_video_viewer"), "%s", error.what());
    rclcpp::shutdown();
    return 1;
  }
  rclcpp::shutdown();
  return 0;
}
