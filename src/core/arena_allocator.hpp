#ifndef ARENA_ALLOCATOR_HPP
#define ARENA_ALLOCATOR_HPP

#include <vector>
#include <cstdint>

struct FreeRange
{
  uint32_t offset;
  uint32_t size;
};

class ArenaAllocator
{
private:
  std::vector<FreeRange> freeRanges;

public:
  explicit ArenaAllocator(uint32_t capacity);

  bool allocate(uint32_t size, uint32_t &offset);
  void free(uint32_t offset, uint32_t size);
};

#endif // ARENA_ALLOCATOR_HPP
