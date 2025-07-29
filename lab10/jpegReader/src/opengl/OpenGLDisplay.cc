#include "OpenGLDisplay.hpp"
#include <chrono>
#include <iostream>
#include <thread>

OpenGLDisplay::OpenGLDisplay()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);

    window = glfwCreateWindow(800, 600, "OpenGL Display", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return;
    }

    glfwMakeContextCurrent(window);
    glEnable(GL_TEXTURE_2D);

    glfwSwapInterval(1);
}

OpenGLDisplay::~OpenGLDisplay()
{
    if (textureID)
    {
        glDeleteTextures(1, &textureID);
    }
    if (window)
    {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

bool OpenGLDisplay::display(const ImageData &image)
{
    if (!image.is_valid() || image.channels != 3)
    {
        std::cerr << "Invalid image data or unsupported channel count (must be 3 for RGB)\n";
        return false;
    }

    if (textureID)
    {
        glDeleteTextures(1, &textureID);
        textureID = 0; // Reset textureID after deletion
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.width, image.height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 image.pixel_data.data());

    int winW, winH;
    glfwGetFramebufferSize(window, &winW, &winH);

    float imgAspect = static_cast<float>(image.width) / image.height;
    float winAspect = static_cast<float>(winW) / winH;

    float scaleX = 1.0f, scaleY = 1.0f;

    if (imgAspect > winAspect)
    {
        scaleY = winAspect / imgAspect;
    }
    else
    {
        scaleX = imgAspect / winAspect;
    }

    glViewport(0, 0, winW, winH);

    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f); // Map [-1,1] range to screen

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glBindTexture(GL_TEXTURE_2D, textureID);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-scaleX, -scaleY);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(scaleX, -scaleY);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(scaleX, scaleY);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-scaleX, scaleY);
    glEnd();

    glfwSwapBuffers(window);
    std::this_thread::sleep_for(std::chrono::seconds(6));
    return true;
}

std::unique_ptr<ImageDisplay> createOpenGLDisplay()
{
    return std::make_unique<OpenGLDisplay>();
}