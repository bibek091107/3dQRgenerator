#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

Camera::Camera() {
    Reset();
}

void Camera::Reset() {
    m_Distance = 15.0f;
    m_Pitch = 45.0f;
    m_Yaw = 45.0f;
    m_Target = glm::vec3(4.5f, 4.5f, 0.0f); // Roughly center of a 9mm QR
}

void Camera::SetTopDownView() {
    m_Distance = 15.0f;
    m_Pitch = 89.0f; // Top-down view
    m_Yaw = 0.0f;
    m_Target = glm::vec3(4.5f, 4.5f, 0.0f);
}

void Camera::ProcessMouseScroll(float yoffset) {
    m_Distance -= yoffset * 0.5f;
    if (m_Distance < 1.0f) m_Distance = 1.0f;
    if (m_Distance > 50.0f) m_Distance = 50.0f;
}

void Camera::ProcessMouseDrag(float deltaX, float deltaY) {
    m_Yaw -= deltaX * 0.5f;
    m_Pitch -= deltaY * 0.5f;

    if (m_Pitch > 89.0f) m_Pitch = 89.0f;
    if (m_Pitch < -89.0f) m_Pitch = -89.0f;
}

void Camera::ProcessMousePan(float deltaX, float deltaY) {
    // Simple pan relative to screen space
    float panSpeed = 0.01f * m_Distance;
    
    glm::vec3 right = glm::vec3(GetViewMatrix()[0][0], GetViewMatrix()[1][0], GetViewMatrix()[2][0]);
    glm::vec3 up = glm::vec3(GetViewMatrix()[0][1], GetViewMatrix()[1][1], GetViewMatrix()[2][1]);

    m_Target += right * deltaX * -panSpeed;
    m_Target += up * deltaY * panSpeed;
}

glm::vec3 Camera::GetPosition() const {
    float rPitch = glm::radians(m_Pitch);
    float rYaw = glm::radians(m_Yaw);

    glm::vec3 pos;
    pos.x = m_Target.x + m_Distance * cos(rPitch) * sin(rYaw);
    pos.y = m_Target.y + m_Distance * sin(rPitch);
    pos.z = m_Target.z + m_Distance * cos(rPitch) * cos(rYaw);
    
    return pos;
}

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(GetPosition(), m_Target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::GetProjectionMatrix(float aspect) const {
    return glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
}
