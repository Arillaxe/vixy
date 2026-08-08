#include <core/arena_allocator.hpp>
#include <algorithm>
#include <assert.h>

ArenaAllocator::ArenaAllocator(uint32_t capacity)
{
  if (capacity > 0)
    freeRanges.push_back({0, capacity});
}

bool ArenaAllocator::allocate(uint32_t size, uint32_t &offset)
{
  assert(size != 0);

  for (size_t i = 0; i < freeRanges.size(); ++i)
  {
    FreeRange &range = freeRanges[i];

    if (range.size < size)
      continue;

    offset = range.offset;

    range.offset += size;
    range.size -= size;

    if (range.size == 0)
      freeRanges.erase(freeRanges.begin() + i);

    return true;
  }

  return false;
}

void ArenaAllocator::free(uint32_t offset, uint32_t size)
{
  assert(size != 0);

  freeRanges.push_back({offset, size});

  std::sort(
      freeRanges.begin(),
      freeRanges.end(),
      [](const FreeRange &a, const FreeRange &b)
      {
        return a.offset < b.offset;
      });

  std::vector<FreeRange> merged;
  merged.reserve(freeRanges.size());

  for (const FreeRange &range : freeRanges)
  {
    if (!merged.empty() &&
        merged.back().offset + merged.back().size == range.offset)
    {
      merged.back().size += range.size;
    }
    else
    {
      merged.push_back(range);
    }
  }

  freeRanges.swap(merged);
}
