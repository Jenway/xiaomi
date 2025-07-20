//
// Created by Jenway on 2025/7/20.
//

#include "glDisplayer.hpp"

namespace glDisplayer ::utils {
    GLuint compileShader(GLenum type, const char *src) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        }
        return shader;
    }

    GLuint createProgram(const char *vertexSrc, const char *fragmentSrc) {
        GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);
        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return program;
    }

    // 创建 VAO + VBO
    void setupQuadGeometry(GLuint &vao, GLuint &vbo) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) 0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              (void *) (2 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }


    void updateTexture(GLuint &texture, const void *imgData, uint32_t imgW, uint32_t imgH) {
        glBindTexture(GL_TEXTURE_2D, texture);

        // 设置纹理参数
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgW, imgH, 0, GL_RGBA, GL_UNSIGNED_BYTE, imgData);
    }

    void render(GLuint program, GLuint vao, GLuint texture) {

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program);
        glBindVertexArray(vao);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        GLint loc = glGetUniformLocation(program, "uTexture");
        glUniform1i(loc, 0);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void renderAt(GLuint program, GLuint vao, GLuint texture, const ImageDesc &img,
                  float screenW, float screenH) {

        float finalW = img.displayWidth;
        float finalH = img.displayHeight;

        float ndcX = (img.posX / screenW) * 2.0f - 1.0f;
        float ndcY = 1.0f - (img.posY / screenH) * 2.0f;

        float ndcDisplayW = (finalW / screenW) * 2.0f;
        float ndcDisplayH = (finalH / screenH) * 2.0f;

        float centerX = ndcX + ndcDisplayW / 2.0f;
        float centerY = ndcY - ndcDisplayH / 2.0f;

        float model[16] = {
                ndcDisplayW, 0.0f,        0.0f, 0.0f,
                0.0f,        ndcDisplayH, 0.0f, 0.0f,
                0.0f,        0.0f,        1.0f, 0.0f,
                centerX,     centerY,     0.0f, 1.0f
        };


        glUseProgram(program);
        glBindVertexArray(vao);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        GLint texLoc = glGetUniformLocation(program, "uTexture");
        glUniform1i(texLoc, 0);

        GLint modelLoc = glGetUniformLocation(program, "uModel");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);

    }




}

