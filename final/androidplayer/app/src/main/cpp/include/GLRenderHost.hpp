#pragma once
#include "Entitys.hpp"
#include <memory>

struct ANativeWindow;

namespace render_utils {
using player_utils::VideoFrame;

class GLRenderHost {
public:
    static std::unique_ptr<GLRenderHost> create();
    ~GLRenderHost();

    // init 现在会启动渲染线程
    bool init(ANativeWindow* window);
    // release 会停止线程并清理资源
    void release();

    void submitFrame(std::shared_ptr<VideoFrame> frame);

    // 清空队列并重置时钟，用于 seek
    void flush();

    // 新增：控制渲染循环的状态
    void pause();
    void resume();

private:
    GLRenderHost();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace render_utils
