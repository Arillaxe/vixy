#include <core/input.hpp>

#include <chrono>
#include <cmath>
#include <format>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

constexpr float CAMERA_SPEED = 500.0f;

Input::Input(Window &win, Camera &cam)
    : window(win),
      camera(cam)
{
  camera.position = glm::vec3(0, 1000, 0);
  camera.rotation = glm::angleAxis(yaw, glm::vec3(0, 1, 0)) * glm::angleAxis(pitch, glm::vec3(1, 0, 0));
  glfwSetInputMode(window.getGLFWWindowHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Input::pollEvents()
{
  static auto startTime = std::chrono::high_resolution_clock::now();
  static auto lastTime = startTime;

  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
  float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();

  lastTime = currentTime;

  static float fpsTimer = 0;
  static int fpsFrames = 0;
  static float fps = 0;

  fpsTimer += deltaTime;
  fpsFrames++;

  if (fpsTimer > 0.25f)
  {
    fps = static_cast<float>(fpsFrames) / fpsTimer;
    fpsTimer = 0;
    fpsFrames = 0;
  }

  std::string cameraPos = std::format(
      "{} FPS {:.2f} {:.2f} {:.2f}",
      static_cast<int>(fps),
      camera.position.x,
      camera.position.y,
      camera.position.z);
  glfwSetWindowTitle(window.getGLFWWindowHandle(), cameraPos.c_str());

  if (glfwGetKey(window.getGLFWWindowHandle(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
  {
    glfwSetWindowShouldClose(window.getGLFWWindowHandle(), true);
  }

  glm::vec3 forward = camera.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 right = camera.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

  if (glfwGetKey(window.getGLFWWindowHandle(), GLFW_KEY_W) == GLFW_PRESS)
  {
    camera.position += forward * CAMERA_SPEED * deltaTime;
  }

  if (glfwGetKey(window.getGLFWWindowHandle(), GLFW_KEY_S) == GLFW_PRESS)
  {
    camera.position -= forward * CAMERA_SPEED * deltaTime;
  }

  if (glfwGetKey(window.getGLFWWindowHandle(), GLFW_KEY_D) == GLFW_PRESS)
  {
    camera.position += right * CAMERA_SPEED * deltaTime;
  }

  if (glfwGetKey(window.getGLFWWindowHandle(), GLFW_KEY_A) == GLFW_PRESS)
  {
    camera.position -= right * CAMERA_SPEED * deltaTime;
  }

  if (glfwGetKey(window.getGLFWWindowHandle(), GLFW_KEY_SPACE) == GLFW_PRESS)
  {
    camera.position += up * CAMERA_SPEED * deltaTime;
  }

  if (glfwGetKey(window.getGLFWWindowHandle(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
  {
    camera.position -= up * CAMERA_SPEED * deltaTime;
  }

  bool toggleKeyDown = glfwGetKey(window.getGLFWWindowHandle(), GLFW_KEY_E) == GLFW_PRESS;
  if (toggleKeyDown && !toggleKeyWasDown)
  {
    glfwSetInputMode(window.getGLFWWindowHandle(), GLFW_CURSOR, mouseFree ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    mouseFree = !mouseFree;
    firstMouse = true;
  }
  toggleKeyWasDown = toggleKeyDown;

  pollMouse();
}

void Input::pollMouse()
{
  if (mouseFree)
    return;

  double mouseX, mouseY;
  glfwGetCursorPos(window.getGLFWWindowHandle(), &mouseX, &mouseY);

  if (firstMouse)
  {
    lastX = mouseX;
    lastY = mouseY;
    firstMouse = false;
  }

  float dx = static_cast<float>(mouseX - lastX);
  float dy = static_cast<float>(mouseY - lastY);

  lastX = mouseX;
  lastY = mouseY;

  constexpr float sensitivity = 0.002f;
  constexpr float maxPitch = glm::radians(89.0f);

  yaw = std::remainder(yaw - dx * sensitivity, glm::two_pi<float>());
  pitch = glm::clamp(pitch - dy * sensitivity, -maxPitch, maxPitch);

  camera.rotation = glm::angleAxis(yaw, glm::vec3(0, 1, 0)) * glm::angleAxis(pitch, glm::vec3(1, 0, 0));
}
