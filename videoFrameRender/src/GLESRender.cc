#include "GLESRender.hpp"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <cassert>
#include <iostream>

namespace mp4parser {

// 简单顶点着色器（使用纹理坐标）
static const char* vertex_shader_src = R"(
    attribute vec4 aPosition;
    attribute vec2 aTexCoord;
    varying vec2 vTexCoord;
    void main() {
        gl_Position = aPosition;
        vTexCoord = aTexCoord;
    }
)";

// 简单片元着色器，采样三个YUV纹理，转换为RGB输出
static const char* fragment_shader_src = R"(
    precision mediump float;
    varying vec2 vTexCoord;
    uniform sampler2D tex_y;
    uniform sampler2D tex_u;
    uniform sampler2D tex_v;

    void main() {
        float y = texture2D(tex_y, vTexCoord).r;
        float u = texture2D(tex_u, vTexCoord).r - 0.5;
        float v = texture2D(tex_v, vTexCoord).r - 0.5;

        float r = y + 1.402 * v;
        float g = y - 0.344 * u - 0.714 * v;
        float b = y + 1.772 * u;

        gl_FragColor = vec4(r, g, b, 1.0);
    }
)";

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(length);
        glGetShaderInfoLog(shader, length, &length, log.data());
        std::cerr << "Shader compile error: " << log.data() << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint create_program(const char* vert_src, const char* frag_src) {
    GLuint vert_shader = compile_shader(GL_VERTEX_SHADER, vert_src);
    if (!vert_shader) return 0;
    GLuint frag_shader = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    if (!frag_shader) {
        glDeleteShader(vert_shader);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert_shader);
    glAttachShader(program, frag_shader);

    glBindAttribLocation(program, 0, "aPosition");
    glBindAttribLocation(program, 1, "aTexCoord");

    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(length);
        glGetProgramInfoLog(program, length, &length, log.data());
        std::cerr << "Program link error: " << log.data() << std::endl;
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    return program;
}

GLESRender::GLESRender() {
    frame_buffer_.resize(kRingBufferSize);
}

GLESRender::~GLESRender() {
    release_gl();
}

bool GLESRender::init_gl() {
    shader_program_ = create_program(vertex_shader_src, fragment_shader_src);
    if (!shader_program_) {
        std::cerr << "Failed to create shader program" << std::endl;
        return false;
    }

    // Setup VAO/VBO for a fullscreen quad
    GLfloat vertices[] = {
        // pos(x,y)    texcoord(u,v)
        -1.f,  1.f,   0.f, 0.f,
        -1.f, -1.f,   0.f, 1.f,
         1.f,  1.f,   1.f, 0.f,
         1.f, -1.f,   1.f, 1.f,
    };

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Note: GLES2 doesn't have VAO by default, if GLES3+可用，使用glGenVertexArrays/glBindVertexArray
    vao_ = 0;

    // Create textures for Y, U, V planes
    glGenTextures(1, &y_tex_);
    glBindTexture(GL_TEXTURE_2D, y_tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &u_tex_);
    glBindTexture(GL_TEXTURE_2D, u_tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &v_tex_);
    glBindTexture(GL_TEXTURE_2D, v_tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return true;
}

void GLESRender::release_gl() {
    if (y_tex_) {
        glDeleteTextures(1, &y_tex_);
        y_tex_ = 0;
    }
    if (u_tex_) {
        glDeleteTextures(1, &u_tex_);
        u_tex_ = 0;
    }
    if (v_tex_) {
        glDeleteTextures(1, &v_tex_);
        v_tex_ = 0;
    }
    if (vbo_) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_) {
        // GLES2没有VAO可以跳过
        vao_ = 0;
    }
    if (shader_program_) {
        glDeleteProgram(shader_program_);
        shader_program_ = 0;
    }
}

void GLESRender::submit_frame(std::shared_ptr<VideoFrame> frame) {
    // 这里只做简单存储，锁保护写入索引并存储到环形缓冲（假设环形缓冲单独实现）
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    frame_buffer_[write_index_].frame = std::move(frame);
    frame_buffer_[write_index_].ready = true;

    write_index_ = (write_index_ + 1) % kRingBufferSize;
    // 读索引不动，render_latest_frame() 读
}

void GLESRender::render_latest_frame() {
    std::shared_ptr<VideoFrame> frame_to_draw;

    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        // 先查找最新准备好的帧，简化：直接读write_index_前一帧
        size_t latest_index = (write_index_ + kRingBufferSize - 1) % kRingBufferSize;

        if (frame_buffer_[latest_index].ready) {
            frame_to_draw = frame_buffer_[latest_index].frame;
            frame_buffer_[latest_index].ready = false;  // 标记已消费
        }
    }

    if (!frame_to_draw)
        return;

    // 上传纹理，绘制
    upload_yuv_to_texture(*frame_to_draw);
    draw_frame();
}

void GLESRender::upload_yuv_to_texture(const VideoFrame& frame) {
    // 假设 frame.format 是 YUV420P，data中连续平面，宽高已知，linesize用于定位每行字节数

    // Y plane
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, y_tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, frame.width, frame.height,
                 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                 frame.data.data());
    // U plane
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, u_tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, frame.width / 2, frame.height / 2,
                 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                 frame.data.data() + frame.linesize[0] * frame.height);
    // V plane
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, v_tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, frame.width / 2, frame.height / 2,
                 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                 frame.data.data() + frame.linesize[0] * frame.height + frame.linesize[1] * (frame.height / 2));

    // 绑定 shader uniform sampler
    glUseProgram(shader_program_);
    glUniform1i(glGetUniformLocation(shader_program_, "tex_y"), 0);
    glUniform1i(glGetUniformLocation(shader_program_, "tex_u"), 1);
    glUniform1i(glGetUniformLocation(shader_program_, "tex_v"), 2);
}

void GLESRender::draw_frame() {
    glUseProgram(shader_program_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glEnableVertexAttribArray(0); // aPosition
    glEnableVertexAttribArray(1); // aTexCoord

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

} // namespace mp4parser
