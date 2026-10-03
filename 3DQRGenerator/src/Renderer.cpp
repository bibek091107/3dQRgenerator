#include "Renderer.h"
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h> // We are using standard OpenGL through GLFW on macOS, 
                        // it includes gl.h or gl3.h
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
// If not on apple, we'd need GLAD, but since user is on macOS:
#include <OpenGL/gl3.h>
#endif

#include <iostream>

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;

out vec3 Normal;
out vec3 FragPos;
out vec3 Color;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;  
    Color = aColor;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;
in vec3 Color;

void main()
{
    // Separate black (QR module) vs white (base) paths
    // Color comes from vertex data: 0,0,0 = module; 1,1,1 = base
    float brightness = Color.r + Color.g + Color.b;
    
    if (brightness < 0.5) {
        // BLACK QR modules — keep them dark, only mild ambient
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
        float diff = max(dot(norm, lightDir), 0.0);
        // Very low ambient + tiny diffuse so sides have shape but top stays near-black
        vec3 result = vec3(0.04) + diff * vec3(0.06);
        FragColor = vec4(result, 1.0);
    } else {
        // WHITE BASE — near-white with gentle shading
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 ambient = vec3(0.55);
        vec3 result  = ambient + diff * vec3(0.45);
        result = clamp(result, vec3(0.0), vec3(1.0));
        FragColor = vec4(result, 1.0);
    }
}
)";

Renderer::Renderer() : m_VAO(0), m_VBO(0), m_ShaderProgram(0), m_VertexCount(0) {
}

Renderer::~Renderer() {
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_ShaderProgram) glDeleteProgram(m_ShaderProgram);
}

unsigned int Renderer::CompileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    return shader;
}

void Renderer::Init() {
    unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    m_ShaderProgram = glCreateProgram();
    glAttachShader(m_ShaderProgram, vertexShader);
    glAttachShader(m_ShaderProgram, fragmentShader);
    glLinkProgram(m_ShaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
}

void Renderer::SetMesh(const Mesh& mesh) {
    std::vector<float> vertices;
    // Format: px, py, pz, nx, ny, nz, cr, cg, cb
    for (const auto& tri : mesh.triangles) {
        vertices.push_back(tri.v1.x); vertices.push_back(tri.v1.y); vertices.push_back(tri.v1.z);
        vertices.push_back(tri.normal.x); vertices.push_back(tri.normal.y); vertices.push_back(tri.normal.z);
        vertices.push_back(tri.color.x); vertices.push_back(tri.color.y); vertices.push_back(tri.color.z);

        vertices.push_back(tri.v2.x); vertices.push_back(tri.v2.y); vertices.push_back(tri.v2.z);
        vertices.push_back(tri.normal.x); vertices.push_back(tri.normal.y); vertices.push_back(tri.normal.z);
        vertices.push_back(tri.color.x); vertices.push_back(tri.color.y); vertices.push_back(tri.color.z);

        vertices.push_back(tri.v3.x); vertices.push_back(tri.v3.y); vertices.push_back(tri.v3.z);
        vertices.push_back(tri.normal.x); vertices.push_back(tri.normal.y); vertices.push_back(tri.normal.z);
        vertices.push_back(tri.color.x); vertices.push_back(tri.color.y); vertices.push_back(tri.color.z);
    }

    m_VertexCount = vertices.size() / 9;

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Color attribute
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void Renderer::Draw(const glm::mat4& view, const glm::mat4& proj) {
    if (m_VertexCount == 0) return;

    glUseProgram(m_ShaderProgram);

    glm::mat4 model = glm::mat4(1.0f); // Identity
    
    unsigned int modelLoc = glGetUniformLocation(m_ShaderProgram, "model");
    unsigned int viewLoc = glGetUniformLocation(m_ShaderProgram, "view");
    unsigned int projLoc = glGetUniformLocation(m_ShaderProgram, "projection");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &proj[0][0]);

    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, m_VertexCount);
    glBindVertexArray(0);
}
