// Vulkan 3DGS - Copyright (c) 2025 Alejandro Amat (github.com/AlejandroAmat) -
// MIT Licensed

#pragma once
#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>
enum class CameraMovement { FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN };
struct CameraUniforms {
  alignas(16) glm::mat4 viewMatrix;
  alignas(16) glm::mat4 projMatrix;
  alignas(16) glm::vec3 camPos;
  alignas(4) float focal_x;
  alignas(4) float focal_y;
  alignas(4) float tan_fovx;
  alignas(4) float tan_fovy;
  alignas(4) int imageWidth;
  alignas(4) int imageHeight;
  alignas(4) int shDegree;
  // Note: padding may be added automatically to align to 16 bytes
};
class Camera {

public:
  Camera(int w, int h, float fov = 45.0f, float aspectRatio = 16.0f / 9.0f,
         float nearPlane = 0.1f, float farPlane = 1000.0f);

  glm::mat4 GetViewMatrix() const;
  glm::mat4 GetProjectionMatrix() const;
  glm::vec3 GetPosition() const { return _pos; }
  glm::vec3 GetFront() const { return _front; }

  void SetMovementSpeed(float speed) { _moveSpeed = speed; }
  void SetMouseSensitivity(float sensitivity) {
    _mouseSensitivity = sensitivity;
  }
  void SetFOV(float newFov) { _fov = newFov; }

  void ProcessMouseMovement(float deltaX, float deltaY,
                            bool constrainPitch = true);
  void ProcessKeyboard(CameraMovement direction, float deltaTime);
  void UpdateAspectRatio(float aspectRatio);
  CameraUniforms getUniforms();

private:
  glm::vec3 _pos, _front, _up, _worldUp, _right;

  float _yaw, _pitch;

  float _fov, _aspectRatio, _nearPlane, _farPlane;

  float _moveSpeed, _mouseSensitivity;

  int _w, _h;

  CameraUniforms _uniforms;
  void UpdateCameraVectors();
};
