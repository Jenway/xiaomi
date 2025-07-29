#ifndef ASCII_ART_DISPLAY_HPP
#define ASCII_ART_DISPLAY_HPP

#include "CharMapper.hpp"
#include "DisplayInterface.hpp"
#include <caca.h>
#include <memory>

enum class AsciiType
{
    charsetASCII,
    charsetUTF8,
    charsetBraille,
    dither
};

// Struct to hold RGB values for clarity
struct RgbColor
{
    uint8_t r, g, b;
};

struct AsciiArtOptions
{
    double aspect_ratio_correction = 2.0;
    AsciiType type_{AsciiType::charsetASCII};
};

struct PixelBlock
{
    uint8_t gray, r, g, b;
    int pixel_count;
};

struct RenderGeometry
{
    float final_scale;
    int render_width;
    int render_height;
    int offset_x;
    int offset_y;
};

class AsciiArtDisplay : public ImageDisplay
{
  public:
    AsciiArtDisplay(const AsciiArtOptions &options = {});
    ~AsciiArtDisplay();

    AsciiArtDisplay(const AsciiArtDisplay &) = delete;
    AsciiArtDisplay &operator=(const AsciiArtDisplay &) = delete;
    AsciiArtDisplay(AsciiArtDisplay &&) = delete;
    AsciiArtDisplay &operator=(AsciiArtDisplay &&) = delete;

    bool display(const ImageData &image);

    bool display_charset(const ImageData &image);
    bool display_dither(const ImageData &image);

  private:
    caca_display_t *display_ = nullptr;
    caca_canvas_t *canvas_ = nullptr;
    AsciiArtOptions options_ = {};

    [[nodiscard]] RenderGeometry computeRenderGeometry(const ImageData &image, int canvas_width,
                                                       int canvas_height) const;
    bool display_charset(const ImageData &image, const CharMapper &mapper);
};

std::unique_ptr<ImageDisplay> createAsciiArtDisplay(int opt);

#endif // ASCII_ART_DISPLAY_HPP