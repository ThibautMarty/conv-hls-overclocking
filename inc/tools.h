#ifndef __TOOLS_H
#define __TOOLS_H

#include <stddef.h> // size_t
#include <stdint.h> // uint64_t in perf_counter

// In a different file for copyright issue
#include "perf_counter.h"

// Some macros
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define UPPERDIV(NUM, DEN) ((NUM + DEN - 1) / DEN)

// Needed to use #define constants in pragmas
#define PRAGMA_SUB(x) _Pragma (#x)
#define DO_PRAGMA(x) PRAGMA_SUB(x)

// Stringify a macro
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// compute base 2 logarithm (works on integer types), rounded up or down
template <typename T>
constexpr T floorlog2(T x)
{
  return (x == 1) ? 0 : 1 + floorlog2(x >> 1);
}
// this is also number of bits needed to represent at most x (excluded)
// e.g if x = 16, how many bits are needed to represent 0..15
template <typename T>
constexpr T ceillog2(T x)
{
  return (x == 1) ? 0 : floorlog2(x - 1) + 1;
}

// Helper functions to access flat arrays as n-D arrays
// e.g to perform array access A[y][x] with dimensions [H][W], do:
// A[get(H, W, y, x)]
inline size_t get(
  size_t d1, size_t d0,
  size_t x1, size_t x0
)
{
  return x1 * d0 + x0;
}

inline size_t get(
  size_t d2, size_t d1, size_t d0,
  size_t x2, size_t x1, size_t x0
)
{
  return x0 + d0 * get(d2, d1, x2, x1);
}

inline size_t get(
  size_t d3, size_t d2, size_t d1, size_t d0,
  size_t x3, size_t x2, size_t x1, size_t x0
)
{
  return x0 + d0 * get(d3, d2, d1, x3, x2, x1);
}

inline size_t get(
  size_t d4, size_t d3, size_t d2, size_t d1, size_t d0,
  size_t x4, size_t x3, size_t x2, size_t x1, size_t x0
)
{
  return x0 + d0 * get(d4, d3, d2, d1, x4, x3, x2, x1);
}

inline size_t get(
  size_t d5, size_t d4, size_t d3, size_t d2, size_t d1, size_t d0,
  size_t x5, size_t x4, size_t x3, size_t x2, size_t x1, size_t x0
)
{
  return x0 + d0 * get(d5, d4, d3, d2, d1, x5, x4, x3, x2, x1);
}

#endif // __TOOLS_H
