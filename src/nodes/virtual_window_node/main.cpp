#include <ros/ros.h>

#include "virtual_window/VirtualWindow.hpp"

using namespace virtual_window;

int main(int argc, char *argv[])
{
  ros::init(argc, argv, "virtual_window_node");

  ros::NodeHandle nh;
  ros::NodeHandle pnh("~");

  VirtualWindow::Options options = {
    .display = ":99",
    .width = 1024,
    .height = 768,
    .depth = 24,
    .no_cursor = true
  };

  if (!pnh.getParam("display", options.display))
  {
    ROS_WARN("Using default display :99");
  }

  int width = options.width;
  if (!pnh.getParam("width", width))
  {
    ROS_INFO("Using default width");
  }
  options.width = width;

  int height = options.height;
  if (!pnh.getParam("height", height))
  {
    ROS_INFO("Using default height");
  }
  options.height = height;

  int depth = options.depth;
  if (!pnh.getParam("depth", depth))
  {
    ROS_INFO("Using default depth");
  }
  options.depth = depth;



  std::shared_ptr<VirtualWindow> window;

  if (argc >= 2)
  {
    std::vector<std::string> arguments;
    for (int i = 2; i < argc; ++i)
    {
      arguments.push_back(argv[i]);
    }

    window = VirtualWindow::run(options, {
      .path = argv[1],
      .arguments = arguments
    });
  }
  else
  {
    window = VirtualWindow::run(options);
  }

  

  const auto frame_pub = nh.advertise<sensor_msgs::Image>("/quori/face/image", 1);

  ros::Rate rate(60.0);

  sensor_msgs::Image frame;
  while (ros::ok())
  {
    window->read(frame);
    ros::spinOnce();
    frame_pub.publish(frame);
    rate.sleep();
  }



  return 0;
}