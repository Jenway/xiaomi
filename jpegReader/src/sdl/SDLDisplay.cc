#include "SDLDisplay.hpp"
#include <iostream>

SDLDisplay::SDLDisplay()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
    }
    window =
        SDL_CreateWindow("SDL Display", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);
    if (!window)
    {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
    }
}

SDLDisplay::~SDLDisplay()
{
    if (texture)
        SDL_DestroyTexture(texture);
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    SDL_Quit();
}

bool SDLDisplay::display(const ImageData &image)
{
    if (!image || image.channels != 3)
    {
        std::cerr << "Invalid image data or unsupported channel count" << std::endl;
        return false;
    }

    if (texture)
    {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, image.width, image.height);
    if (!texture)
    {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_UpdateTexture(texture, nullptr, image.pixel_data.data(), image.width * image.channels);

    int winWidth, winHeight;
    SDL_GetWindowSize(window, &winWidth, &winHeight);

    float imgAspect = static_cast<float>(image.width) / image.height;
    float winAspect = static_cast<float>(winWidth) / winHeight;

    SDL_Rect dstRect;
    if (imgAspect > winAspect)
    {
        dstRect.w = winWidth;
        dstRect.h = static_cast<int>(winWidth / imgAspect);
        dstRect.x = 0;
        dstRect.y = (winHeight - dstRect.h) / 2;
    }
    else
    {
        dstRect.h = winHeight;
        dstRect.w = static_cast<int>(winHeight * imgAspect);
        dstRect.y = 0;
        dstRect.x = (winWidth - dstRect.w) / 2;
    }

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, &dstRect);
    SDL_RenderPresent(renderer);

    SDL_Delay(5000);
    return true;
}

std::unique_ptr<ImageDisplay> createSDLDisplay()
{
    return std::make_unique<SDLDisplay>();
}
