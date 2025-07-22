#pragma once

#include "Mp4Parser.h"  // VideoFrame
#include <mutex>
#include <vector>
#include <optional>

namespace mp4parser {

class GLESRender {
public:
    GLESRender();
    ~GLESRender();

    // 提交一帧（由 Decoder 调用）
    void submit_frame(std::shared_ptr<VideoFrame> frame);

    // Java 定时调用（如 onDrawFrame）
    void render_latest_frame();

    // 初始化 GL 资源
    bool init_gl();
    void release_gl();

private:
    struct FrameSlot {
        std::shared_ptr<VideoFrame> frame;
        bool ready = false;
    };

    static constexpr int kRingBufferSize = 5;
    std::vector<FrameSlot> frame_buffer_;
    size_t write_index_ = 0;
    size_t read_index_ = 0;
    std::mutex buffer_mutex_;

    // OpenGL resources
    GLuint y_tex_ = 0;
    GLuint u_tex_ = 0;
    GLuint v_tex_ = 0;
    GLuint shader_program_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;

    void upload_yuv_to_texture(const VideoFrame& frame);
    void draw_frame();
};

}  // namespace mp4parser

