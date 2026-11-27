#ifndef APP_HPP
#define APP_HPP

#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <core/window.hpp>
#include <renderer/renderer.hpp>
#include <core/scene.hpp>
#include <ecs/entity.hpp>
#include <ecs/components/mesh_component.hpp>
#include <ecs/components/transform_component.hpp>
#include <core/input.hpp>
#include <core/camera.hpp>
#include <core/world.hpp>
#include <core/thread_queue.hpp>
#include <thread>
#include <core/chunk_system.hpp>
#include <renderer/mesh_allocator.hpp>

class App
{
private:
  int HEIGHT = 720;
  int WIDTH = 1280;
  Window window;
  Camera camera;
  Scene scene;
  Input input;
  World world;

  std::thread chunkSystemThread;
  ThreadQueue<ChunkMeshMessage> meshQueue;

  Renderer renderer;
  ChunkSystem chunkSystem;

  Mesh robotMesh;

public:
  App()
      : window(WIDTH, HEIGHT),
        input(window, camera),
        world(123456u),
        renderer(window, scene, camera, meshQueue),
        chunkSystem(renderer.getVkResource(), camera.position, world, meshQueue, renderer.getBuffers()),
        robotMesh(renderer.getVkResource(), "./models/robot.gltf")
  {
    Entity object("object");

    object.addComponent<TransformComponent>();
    object.addComponent<MeshComponent>(robotMesh);

    object.getComponent<TransformComponent>()->setPosition(glm::vec3(0.0f, 0.0f, 100.0f));
    object.getComponent<TransformComponent>()->setScale(glm::vec3(0.5f));

    // scene.addEntity(std::move(object));

    chunkSystemThread = std::thread(&ChunkSystem::run, &chunkSystem);
  }

  void gameplay()
  {
    static bool leftPressed = false;
    static bool rightPressed = false;

    GLFWwindow *handle = window.getGLFWWindowHandle();

    if (glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS && !leftPressed)
    {
      leftPressed = true;
      pushBlockEdit(BlockEdit::Action::Break);
    }
    else if (glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_2) == GLFW_PRESS && !rightPressed)
    {
      rightPressed = true;
      pushBlockEdit(BlockEdit::Action::Place);
    }

    if (glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_1) == GLFW_RELEASE)
    {
      leftPressed = false;
    }

    if (glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_2) == GLFW_RELEASE)
    {
      rightPressed = false;
    }
  }

  // The chunk thread owns the World, so it performs the raycast and the edit.
  void pushBlockEdit(BlockEdit::Action action)
  {
    glm::vec3 forward = camera.rotation * glm::vec3(0.0f, 0.0f, -1.0f);

    chunkSystem.pushEdit({
        .action = action,
        .origin = camera.position,
        .direction = forward,
    });
  }

  void run()
  {
    while (!window.shouldClose())
    {
      window.pollEvents();
      input.pollEvents();
      chunkSystem.setViewerPosition(camera.position);
      gameplay();
      renderer.render();
    }

    chunkSystem.close();
    chunkSystemThread.join();

    renderer.waitIdle();
    // Drop any mesh messages the renderer never applied.
    meshQueue.clear();
  }
};

#endif // APP_HPP
