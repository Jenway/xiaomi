#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>

// 顶点着色器 - 只进行坐标传递
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    
    void main()
    {
        gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
    }
)";

// 片段着色器 - 单一颜色填充
const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    
    void main()
    {
        FragColor = vec4(0.8f, 0.2f, 0.2f, 1.0f); // 红色填充
    }
)";

// 创建星形顶点数据
std::vector<float> createStarVertices(int spikes = 5, float outerRadius = 0.8f, float innerRadius = 0.4f) {
    std::vector<float> vertices;
    vertices.push_back(0.0f); // 中心点
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    
    for (int i = 0; i <= spikes * 2; i++) {
        float radius = (i % 2 == 0) ? outerRadius : innerRadius;
        float angle = glm::radians(90.0f) - i * 2 * glm::radians(180.0f) / spikes;
        float x = radius * cos(angle);
        float y = radius * sin(angle);
        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(0.0f);
    }
    return vertices;
}

// 检查着色器编译错误
void checkShaderErrors(GLuint shader, std::string type) {
    GLint success;
    GLchar infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n";
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n";
        }
    }
}

int main() {
    // 初始化GLFW
    if (!glfwInit()) {
        std::cerr << "GLFW初始化失败" << std::endl;
        return -1;
    }
    
    // 配置GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // 创建窗口
    GLFWwindow* window = glfwCreateWindow(800, 600, "静态星形", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "窗口创建失败" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    
    // 初始化GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "GLEW初始化失败" << std::endl;
        return -1;
    }
    
    // 创建并编译着色器
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    checkShaderErrors(vertexShader, "VERTEX");
    
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    checkShaderErrors(fragmentShader, "FRAGMENT");
    
    // 链接着色器程序
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    checkShaderErrors(shaderProgram, "PROGRAM");
    
    // 删除不再需要的着色器
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    // 创建星形顶点数据
    std::vector<float> vertices = createStarVertices();
    
    // 设置顶点缓冲对象(VBO)和顶点数组对象(VAO)
    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    // 设置顶点属性
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    // 禁用深度测试（2D图形不需要）
    glDisable(GL_DEPTH_TEST);
    
    // 主循环
    while (!glfwWindowShouldClose(window)) {
        // 处理输入
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
        
        // 清除屏幕
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // 使用着色器程序
        glUseProgram(shaderProgram);
        
        // 绘制星形
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLE_FAN, 0, vertices.size() / 3);
        glBindVertexArray(0);
        
        // 交换缓冲并轮询事件
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    // 清理资源
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    
    // 终止GLFW
    glfwTerminate();
    return 0;
}
