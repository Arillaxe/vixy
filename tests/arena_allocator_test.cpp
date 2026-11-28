#include <doctest.h>
#include <core/arena_allocator.hpp>

TEST_CASE("ArenaAllocator allocates sequentially until exhausted")
{
  ArenaAllocator arena(30);
  uint32_t offset = 0;

  REQUIRE(arena.allocate(10, offset));
  CHECK(offset == 0);
  REQUIRE(arena.allocate(10, offset));
  CHECK(offset == 10);
  REQUIRE(arena.allocate(10, offset));
  CHECK(offset == 20);

  offset = 12345;
  CHECK_FALSE(arena.allocate(1, offset));
  CHECK(offset == 12345);
}

TEST_CASE("ArenaAllocator with zero capacity cannot allocate")
{
  ArenaAllocator arena(0);
  uint32_t offset = 0;

  CHECK_FALSE(arena.allocate(1, offset));
}

TEST_CASE("ArenaAllocator rejects allocations larger than the capacity")
{
  ArenaAllocator arena(100);
  uint32_t offset = 0;

  CHECK_FALSE(arena.allocate(101, offset));
  REQUIRE(arena.allocate(100, offset));
  CHECK(offset == 0);
}

TEST_CASE("ArenaAllocator reuses a freed range")
{
  ArenaAllocator arena(30);
  uint32_t a = 0, b = 0, c = 0, offset = 0;

  REQUIRE(arena.allocate(10, a));
  REQUIRE(arena.allocate(10, b));
  REQUIRE(arena.allocate(10, c));

  arena.free(b, 10);

  CHECK_FALSE(arena.allocate(11, offset));
  REQUIRE(arena.allocate(4, offset));
  CHECK(offset == 10);
  REQUIRE(arena.allocate(6, offset));
  CHECK(offset == 14);
  CHECK_FALSE(arena.allocate(1, offset));
}

TEST_CASE("ArenaAllocator coalesces freed ranges")
{
  ArenaAllocator arena(30);
  uint32_t a = 0, b = 0, c = 0, offset = 0;

  REQUIRE(arena.allocate(10, a));
  REQUIRE(arena.allocate(10, b));
  REQUIRE(arena.allocate(10, c));

  SUBCASE("in address order, merging with the previous range")
  {
    arena.free(a, 10);
    arena.free(b, 10);
    arena.free(c, 10);
  }

  SUBCASE("in reverse order, merging with the next range")
  {
    arena.free(c, 10);
    arena.free(b, 10);
    arena.free(a, 10);
  }

  SUBCASE("middle last, merging with both neighbours")
  {
    arena.free(a, 10);
    arena.free(c, 10);
    arena.free(b, 10);
  }

  REQUIRE(arena.allocate(30, offset));
  CHECK(offset == 0);
  CHECK_FALSE(arena.allocate(1, offset));
}

TEST_CASE("ArenaAllocator coalesces a freed block with the untouched tail")
{
  ArenaAllocator arena(100);
  uint32_t a = 0, offset = 0;

  REQUIRE(arena.allocate(30, a));
  arena.free(a, 30);

  REQUIRE(arena.allocate(100, offset));
  CHECK(offset == 0);
}

TEST_CASE("ArenaAllocator does not satisfy a request from fragmented space")
{
  ArenaAllocator arena(30);
  uint32_t a = 0, b = 0, c = 0, offset = 0;

  REQUIRE(arena.allocate(10, a));
  REQUIRE(arena.allocate(10, b));
  REQUIRE(arena.allocate(10, c));

  arena.free(a, 10);
  arena.free(c, 10);

  CHECK_FALSE(arena.allocate(20, offset));
  REQUIRE(arena.allocate(10, offset));
  CHECK(offset == 0);
  REQUIRE(arena.allocate(10, offset));
  CHECK(offset == 20);
  CHECK_FALSE(arena.allocate(1, offset));
}

TEST_CASE("ArenaAllocator uses the first range that fits")
{
  ArenaAllocator arena(100);
  uint32_t a = 0, b = 0, c = 0, d = 0, offset = 0;

  REQUIRE(arena.allocate(5, a));
  REQUIRE(arena.allocate(10, b));
  REQUIRE(arena.allocate(20, c));
  REQUIRE(arena.allocate(10, d));

  arena.free(a, 5);
  arena.free(c, 20);

  REQUIRE(arena.allocate(8, offset));
  CHECK(offset == 15);
  REQUIRE(arena.allocate(5, offset));
  CHECK(offset == 0);
}
