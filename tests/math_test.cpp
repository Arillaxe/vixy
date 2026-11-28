#include <doctest.h>
#include <core/math.hpp>

TEST_CASE("floorDiv rounds toward negative infinity")
{
  CHECK(floorDiv(0, 16) == 0);
  CHECK(floorDiv(15, 16) == 0);
  CHECK(floorDiv(16, 16) == 1);
  CHECK(floorDiv(31, 16) == 1);
  CHECK(floorDiv(-1, 16) == -1);
  CHECK(floorDiv(-15, 16) == -1);
  CHECK(floorDiv(-16, 16) == -1);
  CHECK(floorDiv(-17, 16) == -2);
  CHECK(floorDiv(-32, 16) == -2);
  CHECK(floorDiv(-33, 16) == -3);
}

TEST_CASE("floorMod is never negative")
{
  CHECK(floorMod(0, 16) == 0);
  CHECK(floorMod(15, 16) == 15);
  CHECK(floorMod(16, 16) == 0);
  CHECK(floorMod(17, 16) == 1);
  CHECK(floorMod(-1, 16) == 15);
  CHECK(floorMod(-15, 16) == 1);
  CHECK(floorMod(-16, 16) == 0);
  CHECK(floorMod(-17, 16) == 15);
}

TEST_CASE("floorDiv and floorMod recombine to the original value")
{
  for (int divisor : {1, 3, 10, 16})
  {
    for (int value = -100; value <= 100; ++value)
    {
      const int q = floorDiv(value, divisor);
      const int r = floorMod(value, divisor);

      CHECK(r >= 0);
      CHECK(r < divisor);
      CHECK(q * divisor + r == value);
    }
  }
}
