#ifndef CHUNK_SYSTEM_HPP
#define CHUNK_SYSTEM_HPP

#include <core/thread_queue.hpp>
#include <core/world.hpp>
#include <core/chunk_mesh_generator.hpp>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <chrono>
#include <renderer/vk/vk_resource.hpp>
#include <renderer/mesh_allocator.hpp>

struct IVec2Hash
{
  std::size_t operator()(const glm::ivec2 &v) const
  {
    return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1);
  }
};

// A block break/place request from the main thread. The chunk thread runs the
// raycast itself, because it is the only thread allowed to read the World.
struct BlockEdit
{
  enum class Action
  {
    Break,
    Place,
  };

  Action action;
  glm::vec3 origin;
  glm::vec3 direction;
};

// Runs on its own thread and is the sole owner of the World, the MeshAllocator
// and the mesh allocations. Other threads interact with it only through
// pushEdit(), setViewerPosition(), the mesh queue and the free queue.
class ChunkSystem
{
private:
  VkResource &vkResource;
  World &world;
  ChunkMeshGenerator chunkMeshGenerator;
  ThreadQueue<ChunkMeshMessage> &meshQueue;
  ThreadQueue<MeshAllocation> &freeQueue;
  ThreadQueue<BlockEdit> editQueue;
  std::atomic<bool> shouldClose = false;
  std::mutex wakeMutex;
  std::condition_variable wakeCondition;
  bool wakeRequested = false; // guarded by wakeMutex
  std::mutex viewerMutex;
  glm::vec3 viewerPosition;
  std::unordered_map<glm::ivec3, MeshAllocation> meshAllocations;
  std::pair<vk::raii::Buffer &, vk::raii::Buffer &> buffers;
  MeshAllocator meshAllocator;
  std::unordered_set<glm::ivec2> loadedChunks;

  void update();
  void wake();
  void processEdits();
  void processFrees();
  void applyEdit(const BlockEdit &edit);
  void updateChunk(glm::ivec3 chunkPos);
  void remeshChunk(Chunk &chunk);
  glm::vec3 getViewerPosition();
  std::unordered_set<glm::ivec2> getChunksAround(glm::vec3 &position);

public:
  ChunkSystem(VkResource &resource, glm::vec3 initialViewerPosition, World &w, ThreadQueue<ChunkMeshMessage> &meshQueue, ThreadQueue<MeshAllocation> &freeQueue, std::pair<vk::raii::Buffer &, vk::raii::Buffer &> buffers);

  // Thread-safe. Queues a block edit and wakes the chunk thread, which applies
  // it within about a millisecond, even in the middle of a streaming pass.
  void pushEdit(BlockEdit edit);

  // Thread-safe. Sets the position chunks are streamed around.
  void setViewerPosition(const glm::vec3 &position);

  // Thread entry point. Streams chunks every tick and applies block edits as
  // soon as they arrive, including in the middle of a streaming pass.
  void run();

  // Thread-safe. Asks run() to return as soon as possible.
  void close();
};

#endif // CHUNK_SYSTEM_HPP
