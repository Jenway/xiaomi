//
// Created by Jenway on 2025/7/20.
//

#ifndef RENDERLEARN_GLBITMAPRENDER_HPP
#define RENDERLEARN_GLBITMAPRENDER_HPP

#include <GLES3/gl3.h>
#include "glDisplayer.hpp"

namespace glDisplayer {

    class GlBitmapRenderer {
    public:
        GlBitmapRenderer();

        ~GlBitmapRenderer();

        GlBitmapRenderer(GlBitmapRenderer &) = delete;

        GlBitmapRenderer(GlBitmapRenderer &&) = delete;

        GlBitmapRenderer operator==(GlBitmapRenderer &) = delete;

        GlBitmapRenderer operator==(GlBitmapRenderer &&) = delete;

        void draw(const void *imgData, uint32_t imgW, uint32_t imgH);

        void drawComposite(const ImageDesc &bg, const ImageDesc &fg, int screenWidth, int screenHeight);

    private:
        GLuint program_ = 0;
        GLuint vao_ = 0;
        GLuint vbo_ = 0;
        GLuint texture_ = 0;
        GLuint fgTexture_ = 0;
        GLuint bgTexture_ = 0;
    };

}


#endif //RENDERLEARN_GLBITMAPRENDER_HPP
