#pragma once
#include "Entitys.hpp"
#include "SemQueue.hpp"
#include <memory>

struct ANativeWindow;

namespace render_utils {
using player_utils::VideoFrame;

class GLRenderHost {
public:
    static std::unique_ptr<GLRenderHost> create();
    ~GLRenderHost();

    bool init(ANativeWindow* window);
    void release();
    void pause();
    void resume();
    void submitFrame(std::shared_ptr<VideoFrame> frame);
    void flush();

    // 新增：控制渲染循环的状态

private:
    GLRenderHost();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace render_utils
