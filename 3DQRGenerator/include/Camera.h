#pragma once

#include <glm/glm.hpp>

class Camera {
public:
    Camera();

    void ProcessMouseScroll(float yoffset);
    void ProcessMouseDrag(float deltaX, float deltaY);
    void ProcessMousePan(float deltaX, float deltaY);
    void Reset();
    void SetTopDownView();

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspect) const;
    glm::vec3 GetPosition() const;

private:
    float m_Distance;
    float m_Pitch;
    float m_Yaw;
    glm::vec3 m_Target;
};
