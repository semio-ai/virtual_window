#ifndef _VIRTUAL_WINDOW_VIRTUAL_WINDOW_HPP_
#define _VIRTUAL_WINDOW_VIRTUAL_WINDOW_HPP_

#include <memory>
#include <vector>
#include <string>

#include <sensor_msgs/Image.h>

namespace virtual_window
{
  class VirtualWindowImpl;

  class VirtualWindow
  {
  public:
    typedef std::shared_ptr<VirtualWindow> Ptr;
    typedef std::shared_ptr<const VirtualWindow> ConstPtr;

    struct Command
    {
      std::string path;
      std::vector<std::string> arguments;
    };

    struct Options
    {
      std::string display;
      std::size_t width;
      std::size_t height;
      std::uint8_t depth;
      bool no_cursor;
    };
    
    ~VirtualWindow();

    static Ptr run(const Options &options, const Command &command);
    static Ptr run(const Options &options);

    void read(sensor_msgs::Image &frame);
    
  private:
    VirtualWindow(std::unique_ptr<VirtualWindowImpl> &&impl);

    std::unique_ptr<VirtualWindowImpl> impl_;
  };
}

#endif