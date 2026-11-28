#ifndef INPUT_HPP
#define INPUT_HPP

#include <core/window.hpp>
#include <core/camera.hpp>

class Input
{
private:
  Window &window;
  Camera &camera;
  bool mouseFree = false;
  bool toggleKeyWasDown = false;
  float yaw = 0.0f;
  float pitch = 0.0f;

public:
  Input(Window &win, Camera &cam);

  void pollEvents();
  void pollMouse();
};

#endif // INPUT_HPP
