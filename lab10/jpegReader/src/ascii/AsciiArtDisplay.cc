#include "AsciiArtDisplay.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

AsciiArtDisplay::AsciiArtDisplay(const AsciiArtOptions &options) : options_(options)
{
    display_ = caca_create_display(nullptr);
    if (!display_)
        throw std::runtime_error("无法创建 caca display");
    canvas_ = caca_get_canvas(display_);
}

AsciiArtDisplay::~AsciiArtDisplay()
{
    if (display_)
        caca_free_display(display_);
}

std::unique_ptr<ImageDisplay> createAsciiArtDisplay(int opt)
{
    AsciiArtOptions asciiArtOptions = {2.0, AsciiType::charsetASCII};
    switch (opt)
    {
    case 0:
        asciiArtOptions.type_ = AsciiType::charsetASCII;
        break;
    case 1:
        asciiArtOptions.type_ = AsciiType::dither;
        break;
    case 2:
        asciiArtOptions.type_ = AsciiType::charsetUTF8;
        break;
    case 3:
        asciiArtOptions.type_ = AsciiType::charsetBraille;
    default:
        break;
    }
    return std::make_unique<AsciiArtDisplay>(asciiArtOptions);
}

bool AsciiArtDisplay::display(const ImageData &image)
{
    if (options_.type_ == AsciiType::charsetASCII)
    {
        display_charset(image, AsciiCharMapper());
        return true;
    }
    if (options_.type_ == AsciiType::charsetUTF8)
    {
        display_charset(image, UnicodeRampMapper());
        return true;
    }
    if (options_.type_ == AsciiType::dither)
    {
        display_dither(image);
        return true;
    }
    if (options_.type_ == AsciiType::charsetBraille)
    {
        display_charset(image, BrailleCharMapper());
        return true;
    }

    return false;
}

// // Standard 16 ANSI color palette with their typical RGB values.
static const std::array<RgbColor, 16> kAnsiPalette = {{
    {0, 0, 0},       // BLACK
    {0, 0, 170},     // BLUE
    {0, 170, 0},     // GREEN
    {0, 170, 170},   // CYAN
    {170, 0, 0},     // RED
    {170, 0, 170},   // MAGENTA
    {170, 85, 0},    // BROWN (often rendered as YELLOW)
    {170, 170, 170}, // LIGHTGRAY
    {85, 85, 85},    // DARKGRAY
    {85, 85, 255},   // LIGHTBLUE
    {85, 255, 85},   // LIGHTGREEN
    {85, 255, 255},  // LIGHTCYAN
    {255, 85, 85},   // LIGHTRED
    {255, 85, 255},  // LIGHTMAGENTA
    {255, 255, 85},  // YELLOW
    {255, 255, 255}, // WHITE
}};

// Finds the closest ANSI color using Euclidean distance in RGB space.
// This produces much more accurate color representation.
static uint8_t rgbToAnsiColor(uint8_t r, uint8_t g, uint8_t b)
{
    long min_dist = -1;
    uint8_t best_match = 0;

    for (uint8_t i = 0; i < kAnsiPalette.size(); ++i)
    {
        long dr = r - kAnsiPalette[i].r;
        long dg = g - kAnsiPalette[i].g;
        long db = b - kAnsiPalette[i].b;
        long dist = dr * dr + dg * dg + db * db; // Use squared distance to avoid sqrt

        if (min_dist == -1 || dist < min_dist)
        {
            min_dist = dist;
            best_match = i;
        }
    }
    return best_match;
}

struct grayscaleMap
{
    std::vector<uint8_t> data_;
    uint8_t min_gray = 255;
    uint8_t max_gray = 0;
};

grayscaleMap GetGrayscaleMap(const ImageData &image)
{
    std::vector<uint8_t> grayscale_map(static_cast<size_t>(image.width) * image.height);
    uint8_t min_gray = 255;
    uint8_t max_gray = 0;

    for (int y = 0; y < image.height; ++y)
    {
        for (int x = 0; x < image.width; ++x)
        {
            size_t idx = (static_cast<size_t>(y) * image.width + x) * image.channels;
            uint8_t gray = static_cast<uint8_t>(0.299 * image.pixel_data[idx] + 0.587 * image.pixel_data[idx + 1] +
                                                0.114 * image.pixel_data[idx + 2]);
            grayscale_map[static_cast<size_t>(y) * image.width + x] = gray;
            min_gray = std::min(min_gray, gray);
            max_gray = std::max(max_gray, gray);
        }
    }
    return {grayscale_map, min_gray, max_gray};
}

std::optional<PixelBlock> GetAvgPixel(const ImageData &image, const std::vector<uint8_t> &grayscale_map, int x_start,
                                      int x_end, int y_start, int y_end)
{
    int pixel_count = 0;
    double total_gray = 0;
    double total_r = 0;
    double total_g = 0;
    double total_b = 0;

    for (int iy = y_start; iy < y_end; ++iy)
    {
        for (int ix = x_start; ix < x_end; ++ix)
        {
            size_t pixel_idx = static_cast<size_t>(iy) * image.width + ix;
            total_gray += grayscale_map[pixel_idx];

            size_t color_idx = pixel_idx * image.channels;
            total_r += image.pixel_data[color_idx];
            total_g += image.pixel_data[color_idx + 1];
            total_b += image.pixel_data[color_idx + 2];
            ++pixel_count;
        }
    }

    if (pixel_count == 0)
        return std::nullopt;

    PixelBlock result{};
    result.gray = static_cast<uint8_t>(total_gray / pixel_count);
    result.r = static_cast<uint8_t>(total_r / pixel_count);
    result.g = static_cast<uint8_t>(total_g / pixel_count);
    result.b = static_cast<uint8_t>(total_b / pixel_count);
    result.pixel_count = pixel_count;

    return result;
}

RenderGeometry AsciiArtDisplay::computeRenderGeometry(const ImageData &image, int canvas_width, int canvas_height) const
{
    const float corrected_image_height = static_cast<float>(image.height) / options_.aspect_ratio_correction;
    const float scale_w = static_cast<float>(canvas_width) / image.width;
    const float scale_h = static_cast<float>(canvas_height) / corrected_image_height;
    const float final_scale = std::min(scale_w, scale_h);
    const int render_width = static_cast<int>(image.width * final_scale);
    const int render_height = static_cast<int>(corrected_image_height * final_scale);
    const int offset_x = (canvas_width - render_width) / 2;
    const int offset_y = (canvas_height - render_height) / 2;

    return {final_scale, render_width, render_height, offset_x, offset_y};
}

bool AsciiArtDisplay::display_charset(const ImageData &image, const CharMapper &mapper)
{
    if (!image.is_valid() || image.channels < 3)
        return false;

    caca_clear_canvas(canvas_);
    const int canvas_width = caca_get_canvas_width(canvas_);
    const int canvas_height = caca_get_canvas_height(canvas_);

    auto [grayscale_map, min_gray, max_gray] = GetGrayscaleMap(image);

    const float range = static_cast<float>(max_gray - min_gray);
    const float norm_scale = (range > 0) ? 255.0f / range : 0.0f;
    const float gamma = 0.45f; // Gamma校正值 (1/2.2)，让暗部细节更突出

    auto [final_scale, render_width, render_height, offset_x, offset_y] =
        computeRenderGeometry(image, canvas_width, canvas_height);

    for (int cy = 0; cy < render_height; ++cy)
    {
        for (int cx = 0; cx < render_width; ++cx)
        {
            int x_start = static_cast<int>(cx / final_scale);
            int x_end = static_cast<int>((cx + 1) / final_scale);
            int y_start = static_cast<int>(cy / final_scale * options_.aspect_ratio_correction);
            int y_end = static_cast<int>((cy + 1) / final_scale * options_.aspect_ratio_correction);

            x_end = std::min((unsigned)x_end, image.width);
            y_end = std::min((unsigned)y_end, image.height);

            auto pixelOpt = GetAvgPixel(image, grayscale_map, x_start, x_end, y_start, y_end);

            if (!pixelOpt)
                continue;

            auto [avg_gray, avg_r, avg_g, avg_b, pixel_count] = *pixelOpt;

            // 对比度拉伸 和 Gamma校正
            uint8_t normalized_gray = static_cast<uint8_t>((avg_gray - min_gray) * norm_scale);
            uint8_t gamma_corrected_gray = static_cast<uint8_t>(255.0f * std::pow(normalized_gray / 255.0f, gamma));

            const char *ch = mapper.map(gamma_corrected_gray, avg_r, avg_g, avg_b);
            uint8_t ansi_fg = rgbToAnsiColor(avg_r, avg_g, avg_b);
            caca_set_color_ansi(canvas_, ansi_fg, CACA_BLACK);
            if (mapper.isAscii())
                caca_put_char(canvas_, cx + offset_x, cy + offset_y, *ch);
            else
                caca_put_str(canvas_, cx + offset_x, cy + offset_y, ch);
        }
    }

    caca_refresh_display(display_);
    caca_get_event(display_, CACA_EVENT_KEY_PRESS, nullptr, -1);
    return true;
}

// --- 使用 libcaca 抖动功能重写的 display 方法 ---

bool AsciiArtDisplay::display_dither(const ImageData &image)
{
    // 1. 验证输入图像的有效性
    if (!image.is_valid() || image.channels < 3)
    {
        return false;
    }

    caca_clear_canvas(canvas_);
    const unsigned int canvas_width = caca_get_canvas_width(canvas_);
    const unsigned int canvas_height = caca_get_canvas_height(canvas_);
    const unsigned int image_width = image.width;
    const unsigned int image_height = image.height;

    // 2. 将图像数据转换为 libcaca 接受的 32位 ARGB 像素格式
    std::vector<uint32_t> caca_pixels(image_width * image_height);
    for (unsigned int i = 0; i < image_width * image_height; ++i)
    {
        uint8_t r = image.pixel_data[i * image.channels];
        uint8_t g = image.pixel_data[i * image.channels + 1];
        uint8_t b = image.pixel_data[i * image.channels + 2];
        uint8_t a = (image.channels == 4) ? image.pixel_data[i * image.channels + 3] : 255;
        caca_pixels[i] = (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) |
                         (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b));
    }

    // 3. 创建一个 caca_dither 对象
    caca_dither_t *dither = caca_create_dither(32, image_width, image_height, image_width * sizeof(uint32_t),
                                               0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    if (!dither)
    {
        throw std::runtime_error("无法创建 caca dither 对象");
    }

    // (可选) 设置抖动相关的参数以获得最佳效果
    caca_set_dither_algorithm(dither, "fstein");
    caca_set_dither_charset(dither, "shades");
    caca_set_dither_color(dither, "16");
    caca_set_dither_antialias(dither, "prefilter");

    // 4. 【已修正】计算正确的输出尺寸以保持宽高比
    // 校正终端字符的宽高比 (通常字符高度是宽度的两倍)
    const float corrected_image_height = static_cast<float>(image_height) / options_.aspect_ratio_correction;

    // 计算能适应终端的缩放比例
    const float scale_w = static_cast<float>(canvas_width) / image_width;
    const float scale_h = static_cast<float>(canvas_height) / corrected_image_height;
    const float final_scale = std::min(scale_w, scale_h);

    // 计算在画布上渲染的最终尺寸
    const int render_width = static_cast<int>(image_width * final_scale);
    const int render_height = static_cast<int>(corrected_image_height * final_scale);

    // 计算偏移量以在画布上居中显示
    const int offset_x = (canvas_width - render_width) / 2;
    const int offset_y = (canvas_height - render_height) / 2;

    // 5. 执行核心操作：将位图抖动到画布上计算好的居中区域
    // 现在，函数会将图像无失真地渲染到 (offset_x, offset_y)
    // 位置上，大小为 render_width x render_height 的矩形内。
    caca_dither_bitmap(canvas_, offset_x, offset_y, render_width, render_height, dither, caca_pixels.data());

    // 6. 释放 dither 对象
    caca_free_dither(dither);

    // 7. 刷新屏幕以显示结果，并等待用户输入
    caca_refresh_display(display_);
    caca_get_event(display_, CACA_EVENT_KEY_PRESS, nullptr, -1);

    return true;
}
