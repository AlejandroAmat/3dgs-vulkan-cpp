#pragma once
// #define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>

class WindowManager {

public:
  WindowManager(const std::string wName, const int w = 800, const int h = 600)
      : _wName(wName), _w(w), _h(h){};

  void InitWindow();
  void DestroyWindow();
  bool IsActive() const;
  GLFWwindow *getWindow() const { return _window; }

  ~WindowManager() { DestroyWindow(); }

private:
  GLFWwindow *_window;
  const int _w, _h;
  const std::string _wName;
};
