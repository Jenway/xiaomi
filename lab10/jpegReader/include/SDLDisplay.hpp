#pragma once
#include "DisplayInterface.hpp"
#include "JpegDecoder.hpp" // or where ImageData is declared

#include <SDL2/SDL.h>
#include <memory>

class SDLDisplay : public ImageDisplay
{
  public:
    SDLDisplay();
    ~SDLDisplay();

    bool display(const ImageData &image) override;

  private:
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *texture = nullptr;
};
std::unique_ptr<ImageDisplay> createSDLDisplay();
