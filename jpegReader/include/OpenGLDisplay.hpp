#pragma once
#include "DisplayInterface.hpp"
#include <GLFW/glfw3.h>
#include <memory>

class OpenGLDisplay : public ImageDisplay
{
  public:
    OpenGLDisplay();
    ~OpenGLDisplay();

    bool display(const ImageData &image) override;

  private:
    GLFWwindow *window = nullptr;
    GLuint textureID = 0;
};
std::unique_ptr<ImageDisplay> createOpenGLDisplay();
