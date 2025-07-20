//
// Created by Jenway on 2025/7/20.
//

#include <android/log.h>
#include "glBitmapRender.hpp"
#include "glDisplayer.hpp"

namespace glDisplayer {

    GlBitmapRenderer::GlBitmapRenderer() : program_(utils::createProgram(vertexSrc.data(), fragmentSrc.data())) {
        // program: 初始化 shader
        // 初始化 VAO/VBO
        utils::setupQuadGeometry(vao_, vbo_);

        // 设置参数
        // 开启混合
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        
        // 纹理第一次创建时生成对象
        glGenTextures(1, &texture_);
        glGenTextures(1, &fgTexture_);
        glGenTextures(1, &bgTexture_);
    }

    GlBitmapRenderer::~GlBitmapRenderer() {
        auto safeGlDelete = [](GLuint &id, auto glDeleteFunc) {
            if (id != 0) {
                glDeleteFunc(1, &id);
                id = 0;
            }
        };

        // Use the lambdas to clean up
        safeGlDelete(texture_, glDeleteTextures); 
        safeGlDelete(fgTexture_, glDeleteTextures);
        safeGlDelete(bgTexture_, glDeleteTextures);
        safeGlDelete(vbo_, glDeleteBuffers);
        safeGlDelete(vao_, glDeleteVertexArrays); 

        if (program_ != 0) {
            glDeleteProgram(program_);
            program_ = 0;
        }
    }

    void GlBitmapRenderer::draw(const void *imgData, uint32_t imgW, uint32_t imgH) {
        __android_log_print(ANDROID_LOG_DEBUG, "GlBitmapRenderer", "draw called: imgW=%u, imgH=%u",
                            imgW, imgH);

        utils::updateTexture(texture_, imgData, imgW, imgH);
        utils::render(program_, vao_, texture_);
    }

    void GlBitmapRenderer::drawComposite(const ImageDesc &bg, const ImageDesc &fg, int screenWidth,
                                         int screenHeight) {
        utils::updateTexture(bgTexture_, bg.data, bg.imgWidth, bg.imgHeight);
        utils::updateTexture(fgTexture_, fg.data, fg.imgWidth, fg.imgHeight);

        glClearColor(0.0, 0.0, 0.0, 1.0);  // 黑色背景
        glClear(GL_COLOR_BUFFER_BIT);

        // 渲染背景
        utils::renderAt(program_, vao_, bgTexture_, bg,
                        static_cast<float>(screenWidth), static_cast<float>(screenHeight));
        // 渲染前景
        utils::renderAt(program_, vao_, fgTexture_, fg,
                        static_cast<float>(screenWidth), static_cast<float>(screenHeight));

    }


}
