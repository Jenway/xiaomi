//
// Created by Jenway on 2025/7/20.
//

#ifndef RENDERLEARN_GLDISPLAYER_HPP
#define RENDERLEARN_GLDISPLAYER_HPP

#include <GLES3/gl3.h>
#include <string>

namespace glDisplayer {

    struct ImageDesc {
        const void *data;
        uint32_t imgWidth;
        uint32_t imgHeight;
        float posX;       // 目标位置，屏幕像素坐标系，左上角为(0,0)
        float posY;
        float displayWidth;  // 显示大小（像素）,这里传屏幕的大小
        float displayHeight;
    };

//    Constexpr
    inline constexpr std::string_view vertexSrc = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
uniform mat4 uModel;
out vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = uModel * vec4(aPos, 0.0, 1.0);
}
)";

    inline constexpr std::string_view fragmentSrc = R"(#version 300 es
        precision mediump float;
        in vec2 vTexCoord;
        uniform sampler2D uTexture;
        out vec4 outColor;
        void main() {
            outColor = texture(uTexture, vTexCoord);
        })";

    inline float vertices[] = {
            // pos       // tex
            -1.0f, -1.0f, 0.0f, 1.0f,
            1.0f, -1.0f, 1.0f, 1.0f,
            -1.0f, 1.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 1.0f, 0.0f,
    };


    namespace utils {
//        Create Shader
        GLuint createProgram(const char *vertexSrc, const char *fragmentSrc);

        GLuint compileShader(GLenum type, const char *src);

//        Create VAO/VBO
        void setupQuadGeometry(GLuint &vao, GLuint &vbo);

//        update Texture
        void updateTexture(GLuint &texture, const void *imgData, uint32_t imgW, uint32_t imgH);

//    render
        void render(GLuint program, GLuint vao, GLuint texture);

        void renderAt(GLuint program, GLuint vao, GLuint texture, const ImageDesc &img,
                      float screenW, float screenH);
    }

}
#endif //RENDERLEARN_GLDISPLAYER_HPP
