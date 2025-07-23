#include "GLESRender.hpp"

#include <cassert>
#include <iostream>

namespace render_utils {
namespace {
    GLuint compile_shader(GLenum type, const char* src);
    GLuint create_program(const char* vert_src, const char* frag_src);
    void setupQuadGeometry(GLuint& vao, GLuint& vbo);
}

constexpr const char* vertex_shader_src = R"(
    attribute vec4 aPosition;
    attribute vec2 aTexCoord;
    uniform mat4 u_mvpMatrix;
    varying vec2 vTexCoord;
    void main() {
        gl_Position = u_mvpMatrix * aPosition;
        vTexCoord = aTexCoord;
    }
)";

constexpr const char* fragment_shader_src = R"(
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

GLESRender::GLESRender()
{
    init();
}

GLESRender::~GLESRender()
{
    release_gl();
}

bool GLESRender::init()
{
    shader_program_ = create_program(vertex_shader_src, fragment_shader_src);
    if (shader_program_ == 0U) {
        std::cerr << "Failed to create shader program" << '\n';
        return false;
    }
    mvp_matrix_location_ = glGetUniformLocation(shader_program_, "u_mvpMatrix");
    glUseProgram(shader_program_);
    glUniform1i(glGetUniformLocation(shader_program_, "tex_y"), 0);
    glUniform1i(glGetUniformLocation(shader_program_, "tex_u"), 1);
    glUniform1i(glGetUniformLocation(shader_program_, "tex_v"), 2);
    glUseProgram(0);
    setupQuadGeometry(vao_, vbo_);

    auto init_plane_texture = [](GLuint& tex) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };

    init_plane_texture(y_tex_);
    init_plane_texture(u_tex_);
    init_plane_texture(v_tex_);

    return true;
}

void GLESRender::release_gl()
{
    if (y_tex_ != 0U) {
        glDeleteTextures(1, &y_tex_);
        y_tex_ = 0;
    }
    if (u_tex_ != 0U) {
        glDeleteTextures(1, &u_tex_);
        u_tex_ = 0;
    }
    if (v_tex_ != 0U) {
        glDeleteTextures(1, &v_tex_);
        v_tex_ = 0;
    }
    if (vbo_ != 0U) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0U) {
        // GLES2没有VAO可以跳过
        vao_ = 0;
    }
    if (shader_program_ != 0U) {
        glDeleteProgram(shader_program_);
        shader_program_ = 0;
    }
}

void GLESRender::paint(const std::shared_ptr<VideoFrame>& frame_to_draw)
{
    if (!frame_to_draw || frame_to_draw->width == 0 || frame_to_draw->height == 0 || viewport_width_ == 0 || viewport_height_ == 0) {
        // 清除屏幕为黑色，避免残留上一帧
        glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    // 激活着色器程序
    glUseProgram(shader_program_);

    // 1. 计算变换矩阵
    float frame_aspect = static_cast<float>(frame_to_draw->width) / frame_to_draw->height;
    float viewport_aspect = static_cast<float>(viewport_width_) / viewport_height_;

    float scale_x = 1.0F;
    float scale_y = 1.0F;

    if (frame_aspect > viewport_aspect) {
        scale_y = viewport_aspect / frame_aspect;
    } else {
        scale_x = frame_aspect / viewport_aspect;
    }

    const float mvpMatrix[16] = {
        scale_x, 0.0F, 0.0F, 0.0F,
        0.0F, scale_y, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F
    };

    // 2. 上传矩阵
    glUniformMatrix4fv(mvp_matrix_location_, 1, GL_FALSE, mvpMatrix);

    // 3. 上传YUV纹理数据
    upload_yuv_to_texture(*frame_to_draw);

    // 4. 清屏并绘制
    glClearColor(0.0F, 0.0F, 0.0F, 1.0F); // 设置黑边颜色
    glClear(GL_COLOR_BUFFER_BIT);
    draw_frame();

    // 绘制完成后取消激活
    glUseProgram(0);
}

void GLESRender::upload_yuv_to_texture(const VideoFrame& frame)
{

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
}

void GLESRender::draw_frame()
{
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

void GLESRender::on_viewport_change(int width, int height)
{
    viewport_width_ = width;
    viewport_height_ = height;
    glViewport(0, 0, width, height); // 通常也在这里设置视口
}

// ======================== Utils ===========================
namespace {
    GLuint compile_shader(GLenum type, const char* src)
    {
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

    GLuint create_program(const char* vert_src, const char* frag_src)
    {
        GLuint vert_shader = compile_shader(GL_VERTEX_SHADER, vert_src);
        if (vert_shader == 0U) {
            return 0;
        }
        GLuint frag_shader = compile_shader(GL_FRAGMENT_SHADER, frag_src);
        if (frag_shader == 0U) {
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
        if (linked == 0) {
            GLint length = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            std::vector<char> log(length);
            glGetProgramInfoLog(program, length, &length, log.data());
            std::cerr << "Program link error: " << log.data() << '\n';
            glDeleteProgram(program);
            program = 0;
        }

        glDeleteShader(vert_shader);
        glDeleteShader(frag_shader);

        return program;
    }

    void setupQuadGeometry(GLuint& vao, GLuint& vbo)
    {
        const GLfloat vertices[] = {
            // pos(x,y)    texcoord(u,v)
            -1.f,
            1.f,
            0.f,
            0.f,
            -1.f,
            -1.f,
            0.f,
            1.f,
            1.f,
            1.f,
            1.f,
            0.f,
            1.f,
            -1.f,
            1.f,
            1.f,
        };

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        constexpr GLint pos_loc = 0;
        constexpr GLint tex_loc = 1;
        constexpr GLsizei stride = 4 * sizeof(GLfloat);

        glEnableVertexAttribArray(pos_loc);
        glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));

        glEnableVertexAttribArray(tex_loc);
        glVertexAttribPointer(tex_loc, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(GLfloat)));

        glBindVertexArray(0); // 解绑 VAO
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

} // namespace

} // namespace render_utils
