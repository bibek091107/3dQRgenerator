#pragma once

#include "Mesh.h"
#include <glm/glm.hpp>

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Initialize OpenGL state (shaders, etc.)
    void Init();

    // Upload a new mesh to the GPU
    void SetMesh(const Mesh& mesh);

    // Render the current mesh
    void Draw(const glm::mat4& view, const glm::mat4& proj);

private:
    unsigned int m_VAO, m_VBO;
    unsigned int m_ShaderProgram;
    int m_VertexCount;

    unsigned int CompileShader(unsigned int type, const char* source);
};
