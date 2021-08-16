# virtual_window

A ROS node (and library) that allows a X11 window to be started in a headless session and published to a ROS image. This can be useful for things like robot faces that need to be transformed before being drawn to screen (e.g., projected with a mirror).

## Dependencies

  - ROS Noetic (untested on other ROSes, but should work)
  - `Xvfb` (expected to be in PATH)

## Usage

```sh
# Create a headless X11 session on display :99 with a 512x512 px drawing area (default depth of 24 bits)
rosrun virtual_window virtual_window_node _display:=:99 _width:=512 _height:=512

# Run the application of your choice on the headless session
DISPLAY=:99 my_gui_app --fullscreen
```

## License 

Licensed under the terms of the BSD 3-Clause license.
