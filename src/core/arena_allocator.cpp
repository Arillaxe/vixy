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

  auto next = std::lower_bound(
      freeRanges.begin(),
      freeRanges.end(),
      offset,
      [](const FreeRange &range, uint32_t value)
      {
        return range.offset < value;
      });

  bool mergePrev = false;
  bool mergeNext = false;

  if (next != freeRanges.begin())
  {
    const FreeRange &prev = *(next - 1);
    assert(prev.offset + prev.size <= offset);
    mergePrev = prev.offset + prev.size == offset;
  }

  if (next != freeRanges.end())
  {
    assert(offset + size <= next->offset);
    mergeNext = offset + size == next->offset;
  }

  if (mergePrev && mergeNext)
  {
    (next - 1)->size += size + next->size;
    freeRanges.erase(next);
  }
  else if (mergePrev)
  {
    (next - 1)->size += size;
  }
  else if (mergeNext)
  {
    next->offset = offset;
    next->size += size;
  }
  else
  {
    freeRanges.insert(next, {offset, size});
  }
}
