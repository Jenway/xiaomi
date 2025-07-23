#pragma once

#include "EGLCore.hpp"
#include "Mp4Parser.hpp"
#include "RingBuffer.hpp"
#include "SemQueue.hpp"
#include <GLES3/gl3.h>
#include <cstddef>
#include <memory>
#include <optional>

namespace render_utils {
using mp4parser::VideoFrame;
using player_utils::SemQueue;

class GLESRender {
public:
    static std::optional<std::unique_ptr<GLESRender>> create()
    {
        auto render = std::unique_ptr<GLESRender>(new GLESRender());
        if (!render->init()) {
            return std::nullopt;
        }
        return std::move(render);
    }
    ~GLESRender();

    bool init();
    void paint(const std::shared_ptr<VideoFrame>& frame_to_draw);
    void on_viewport_change(int width, int height);

private:
    GLESRender();

    // OpenGL resources
    GLuint y_tex_ = 0;
    GLuint u_tex_ = 0;
    GLuint v_tex_ = 0;
    GLuint shader_program_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;

    int viewport_width_ = 0;
    int viewport_height_ = 0;
    GLint mvp_matrix_location_ = -1;

    void upload_yuv_to_texture(const VideoFrame& frame);
    void draw_frame();

    void release_gl();
};

} // namespace render_utils
