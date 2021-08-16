#include "virtual_window/VirtualWindow.hpp"

#include <boost/process.hpp>
#include <boost/endian/conversion.hpp>

#include <X11/XWDFile.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <fcntl.h>

#include <sensor_msgs/image_encodings.h>

#include <memory>
#include <iostream>

namespace virtual_window
{
  class VirtualWindowImpl
  {
  public:
  VirtualWindowImpl(const VirtualWindow::Options &options)
      : framebuffer_(nullptr)
    {
      using namespace boost::process;
      using namespace std;

      std::ostringstream o;
      o << options.width << "x" << options.height << "x" << static_cast<std::size_t>(options.depth);

      vector<string> xvfb_arguments {
        options.display,
        "-screen",
        "0",
        o.str(),
        "-shmem",
      };
      if (options.no_cursor) xvfb_arguments.push_back("-nocursor");


      for (const auto &arg : xvfb_arguments)
      {
        std::cout << arg << " ";
      }
      std::cout << std::endl;

    
      
      xvfb_ = make_unique<child>(search_path("Xvfb"), xvfb_arguments, std_err > xvfb_out_);


      const auto cleanup = [&]() {
        xvfb_->terminate();
        xvfb_->wait();
        xvfb_.reset();
      };


      // xvfb will output a line of the form "screen 0 shmid $FD", where $FD is the file descriptor
      // for the shared memory.
      string line;
      if (!xvfb_out_ || !getline(xvfb_out_, line))
      {
        cleanup();
        throw runtime_error("Failed to get shared memory information from xvfb");
      }

      // An error occurred.
      if (line.find("(EE)") != string::npos)
      {
        do
        {
          cout << line << endl;
        } while(xvfb_out_ && getline(xvfb_out_, line));
        
        cleanup();
        throw runtime_error("xvfb encountered an error");
      }


      const static string SHMID = "shmid";

      const auto shmid_start = line.find(SHMID);
      if (shmid_start == string::npos)
      {
        cleanup();
        throw runtime_error("xvfb output is malformed");
      }

      const auto id_start = shmid_start + SHMID.size() + 1;
      const int shmid = std::stoi(line.substr(id_start, line.size() - id_start));

      shmid_ds stat;
      if (shmctl(shmid, IPC_STAT, &stat) < 0)
      {
        cleanup();
        throw runtime_error(strerror(errno));
      }

      // Memory map the shared memory region
      framebuffer_ = reinterpret_cast<uint8_t *>(shmat(shmid, nullptr, SHM_RDONLY));
      if (!framebuffer_)
      {
        cleanup();
        throw runtime_error(strerror(errno));
      }
    }

    VirtualWindowImpl(const VirtualWindow::Options &options, const VirtualWindow::Command &command)
      : framebuffer_(nullptr)
    {
      using namespace boost::process;
      using namespace std;

      std::ostringstream o;
      o << options.width << "x" << options.height << "x" << options.depth;

      vector<string> xvfb_arguments {
        options.display,
        "-screen",
        "0",
        o.str(),
        "-shmem",
      };

      if (options.no_cursor) xvfb_arguments.push_back("-nocursor");
    
      
      xvfb_ = make_unique<child>(search_path("Xvfb"), xvfb_arguments, std_err > xvfb_out_);
      command_ = make_unique<child>(search_path(command.path), command.arguments, env["DISPLAY"] = options.display);


      const auto cleanup = [&]() {
        xvfb_->terminate();
        xvfb_->wait();
        xvfb_.reset();

        command_->terminate();
        command_->wait();
        command_.reset();
      };


      // xvfb will output a line of the form "screen 0 shmid $FD", where $FD is the file descriptor
      // for the shared memory.
      string line;
      if (!xvfb_out_ || !getline(xvfb_out_, line))
      {
        cleanup();
        throw runtime_error("Failed to get shared memory information from xvfb");
      }

      // An error occurred.
      if (line.find("(EE)") != string::npos)
      {
        do
        {
          cout << line << endl;
        } while(xvfb_out_ && getline(xvfb_out_, line));
        
        cleanup();
        throw runtime_error("xvfb encountered an error");
      }


      const static string SHMID = "shmid";

      const auto shmid_start = line.find(SHMID);
      if (shmid_start == string::npos)
      {
        cleanup();
        throw runtime_error("xvfb output is malformed");
      }

      const auto id_start = shmid_start + SHMID.size() + 1;
      const int shmid = std::stoi(line.substr(id_start, line.size() - id_start));

      shmid_ds stat;
      if (shmctl(shmid, IPC_STAT, &stat) < 0)
      {
        cleanup();
        throw runtime_error(strerror(errno));
      }

      // Memory map the shared memory region
      framebuffer_ = reinterpret_cast<uint8_t *>(shmat(shmid, nullptr, SHM_RDONLY));
      if (!framebuffer_)
      {
        cleanup();
        throw runtime_error(strerror(errno));
      }
    }

    void read(sensor_msgs::Image &frame)
    {
      if (!framebuffer_) return;

      using namespace boost::endian;

      const XWDFileHeader *const header = reinterpret_cast<const XWDFileHeader *>(framebuffer_);

      const auto header_size = big_to_native(header->header_size);
      const auto pixmap_width = big_to_native(header->pixmap_width);
      const auto pixmap_height = big_to_native(header->pixmap_height);
      const auto pixmap_depth = big_to_native(header->pixmap_depth);
      const auto pixmap_format = big_to_native(header->pixmap_format);
      const auto bytes_per_line = big_to_native(header->bytes_per_line);
      const auto bits_per_pixel = big_to_native(header->bits_per_pixel);
      const auto colormap_entries = big_to_native(header->colormap_entries);
      const auto visual_class = big_to_native(header->visual_class);
      
      const auto elem_size = 3;

      frame.width = pixmap_width;
      frame.height = pixmap_height;
      frame.step = frame.width * elem_size;

      const size_t logical_size = frame.width * frame.height;
      const size_t physical_size = logical_size * elem_size;
      
      frame.encoding = sensor_msgs::image_encodings::RGB8;
      frame.data.resize(physical_size);

      const XWDColor *const colormaps = reinterpret_cast<const XWDColor *>(framebuffer_ + header_size);

      const std::uint8_t *const pixels = framebuffer_ + header_size + colormap_entries * sizeof(XWDColor);
      switch (visual_class)
      {
        case 4: {
          size_t i = 0;
          for (size_t y = 0; y < pixmap_height; ++y)
          {
            const uint8_t *const row = pixels + y * bytes_per_line; 
            for (size_t x = 0; x < pixmap_width; ++x)
            {
              const uint8_t *const elem = row + x * (bits_per_pixel / 8);
              frame.data[++i] = elem[0];
              frame.data[++i] = elem[1];
              frame.data[++i] = elem[2];
            }
          }
          break;
        }
      }

      
    }

    ~VirtualWindowImpl()
    {
      if (framebuffer_)
      {
        if (shmdt(framebuffer_) < 0)
        {
          perror("shmdt");
        }
      }

      if (command_)
      {
        command_->terminate();
        command_->wait();
        command_.reset();
      }

      if (xvfb_)
      {
        xvfb_->terminate();
        xvfb_->join();
        xvfb_.reset();
      }
    }

  private:
    boost::process::ipstream xvfb_out_;
    std::unique_ptr<boost::process::child> xvfb_;
    std::unique_ptr<boost::process::child> command_;
    std::uint8_t *framebuffer_;
  };
}

using namespace virtual_window;

VirtualWindow::~VirtualWindow()
{

}

VirtualWindow::Ptr VirtualWindow::run(const VirtualWindow::Options &options, const VirtualWindow::Command &command)
{
  return Ptr(new VirtualWindow(std::make_unique<VirtualWindowImpl>(options, command)));
}

VirtualWindow::Ptr VirtualWindow::run(const VirtualWindow::Options &options)
{
  return Ptr(new VirtualWindow(std::make_unique<VirtualWindowImpl>(options)));
}

void VirtualWindow::read(sensor_msgs::Image &frame)
{
  impl_->read(frame);
}

    
VirtualWindow::VirtualWindow(std::unique_ptr<VirtualWindowImpl> &&impl)
  : impl_(std::move(impl))
{
}