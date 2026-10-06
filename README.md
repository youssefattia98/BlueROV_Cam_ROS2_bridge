# BlueROV Camera ROS 2 Bridge

A ROS 2 C++ package that receives the BlueROV H.264 camera stream over
RTP/UDP, decodes it with GStreamer through OpenCV, and displays the live video.
It can optionally publish every decoded frame as a ROS 2 image topic for use by
recording, perception, or visualization nodes.

## Features

- Receives the standard H.264/RTP BlueROV stream on a configurable local IP
  address and UDP port.
- Displays decoded video in a resizable OpenCV window.
- Optionally publishes `sensor_msgs/msg/Image` frames on `/image_raw` using
  sensor-data QoS.
- Drops old buffered frames to keep latency low.
- Accepts a completely custom GStreamer pipeline, including NVIDIA Jetson
  hardware decoding.
- Press `q` or Escape in the video window to stop the node.

## Requirements

- Ubuntu with ROS 2 Jazzy
- A C++17 compiler
- OpenCV with GStreamer support
- GStreamer H.264/RTP plugins

Install the required Ubuntu packages:

```bash
sudo apt update
sudo apt install libopencv-dev gstreamer1.0-tools \
  gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad gstreamer1.0-libav
```

## Clone and build

Clone the package into the `src` directory of a ROS 2 workspace:

```bash
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src
git clone git@github.com:youssefattia98/BlueROV_Cam_ROS2_bridge.git bluerov_video_viewer

cd ~/ros2_ws
source /opt/ros/jazzy/setup.bash
colcon build --packages-select bluerov_video_viewer
source install/setup.bash
```

Use the HTTPS clone URL instead if GitHub SSH keys are not configured:

```bash
git clone https://github.com/youssefattia98/BlueROV_Cam_ROS2_bridge.git bluerov_video_viewer
```

## Configuration

Edit [`config/bluerov_video.yaml`](config/bluerov_video.yaml):

```yaml
bluerov_video_viewer:
  ros__parameters:
    ip_address: "0.0.0.0"
    port: 5600
    pipeline: ""
    window_name: "BlueROV camera"
    frame_id: "bluerov_camera"
    show_window: true
    publish_ros_topic: true
```

| Parameter | Default | Description |
| --- | --- | --- |
| `ip_address` | `0.0.0.0` | Local computer interface on which UDP packets are received. |
| `port` | `5600` | Local UDP port carrying the H.264/RTP stream. |
| `pipeline` | empty | Complete custom GStreamer pipeline. Empty uses the generated default pipeline. |
| `window_name` | `BlueROV camera` | Title of the display window. |
| `frame_id` | `bluerov_camera` | Frame ID assigned to published image messages. |
| `show_window` | `true` | Enables or disables the OpenCV display window. |
| `publish_ros_topic` | `true` | Enables or disables publication of `/image_raw`. |

`ip_address` is the address of the receiving network interface on the computer,
not the remote BlueROV address. Keep it at `0.0.0.0` to listen on every local
interface. Set it to a specific local address only when the computer has
multiple interfaces and the stream must be received on one of them.

## Run

Start the BlueROV camera stream, source the workspace, and launch the node:

```bash
cd ~/ros2_ws
source /opt/ros/jazzy/setup.bash
source install/setup.bash
ros2 launch bluerov_video_viewer bluerov_video_viewer.launch.py
```

Parameters can also be overridden from the command line. For example, listen
on local address `192.168.2.1`, use port `5601`, show the video, and disable ROS
image publication:

```bash
ros2 run bluerov_video_viewer bluerov_video_viewer_node --ros-args \
  -p ip_address:=192.168.2.1 \
  -p port:=5601 \
  -p publish_ros_topic:=false
```

For a headless ROS image bridge without a display window:

```bash
ros2 run bluerov_video_viewer bluerov_video_viewer_node --ros-args \
  -p show_window:=false \
  -p publish_ros_topic:=true
```

## ROS 2 interface

When `publish_ros_topic` is enabled, the node publishes:

```text
/image_raw  sensor_msgs/msg/Image  (BGR8)
```

Confirm that frames are being published:

```bash
ros2 topic hz /image_raw
```

The stream can also be viewed with a ROS image viewer:

```bash
ros2 run rqt_image_view rqt_image_view /image_raw
```

## Jetson hardware decoding

On a Jetson with the NVIDIA GStreamer plugins installed, replace the generated
pipeline by setting `pipeline` in the YAML file, or pass one directly:

```bash
ros2 run bluerov_video_viewer bluerov_video_viewer_node --ros-args \
  -p pipeline:='udpsrc address=0.0.0.0 port=5600 caps="application/x-rtp,media=video,encoding-name=H264,payload=96" ! rtph264depay ! nvv4l2decoder ! nvvidconv ! video/x-raw,format=BGRx ! videoconvert ! video/x-raw,format=BGR ! appsink max-buffers=1 drop=true sync=false'
```

Any custom pipeline must end with an `appsink` that supplies BGR frames to
OpenCV.

## Troubleshooting

Check whether UDP packets are reaching the computer:

```bash
sudo tcpdump -ni any udp port 5600
```

Check the required GStreamer elements:

```bash
gst-inspect-1.0 udpsrc
gst-inspect-1.0 rtph264depay
gst-inspect-1.0 avdec_h264
```

If there is no video:

1. Confirm that the BlueROV is sending H.264/RTP to the computer's IP and the
   configured port.
2. Allow the UDP port through the computer firewall.
3. Keep `ip_address` at `0.0.0.0` unless a specific local interface is needed.
4. Check that OpenCV reports GStreamer support.
5. Verify that another application is not already using the same UDP port.

## License

This project is licensed under the [MIT License](LICENSE).
