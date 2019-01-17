#include <gtest/gtest.h>

#include "tools.h"
#include <ap_int.h>

TEST(ToolTests, Contiguity)
{
  size_t x0, x1, x2, x3, x4, x5;
  size_t D5 = 2, D4 = 3, D3 = 4, D2 = 5, D1 = 6, D0 = 7;
  size_t old, current;

  old = -1;
  for(x5 = 0; x5 < D5; x5++) {
    for(x4 = 0; x4 < D4; x4++) {
      for(x3 = 0; x3 < D3; x3++) {
        for(x2 = 0; x2 < D2; x2++) {
          for(x1 = 0; x1 < D1; x1++) {
            for(x0 = 0; x0 < D0; x0++) {
              current = get(
                D5, D4, D3, D2, D1, D0,
                x5, x4, x3, x2, x1, x0
              );
              EXPECT_EQ(old + 1, current);
              old = current;
            }
          }
        }
      }
    }
  }

  old = -1;
  for(x4 = 0; x4 < D4; x4++) {
    for(x3 = 0; x3 < D3; x3++) {
      for(x2 = 0; x2 < D2; x2++) {
        for(x1 = 0; x1 < D1; x1++) {
          for(x0 = 0; x0 < D0; x0++) {
            current = get(
              D4, D3, D2, D1, D0,
              x4, x3, x2, x1, x0
            );
            EXPECT_EQ(old + 1, current);
            old = current;
          }
        }
      }
    }
  }

  old = -1;
  for(x3 = 0; x3 < D3; x3++) {
    for(x2 = 0; x2 < D2; x2++) {
      for(x1 = 0; x1 < D1; x1++) {
        for(x0 = 0; x0 < D0; x0++) {
          current = get(
            D3, D2, D1, D0,
            x3, x2, x1, x0
          );
          EXPECT_EQ(old + 1, current);
          old = current;
        }
      }
    }
  }

  old = -1;
  for(x2 = 0; x2 < D2; x2++) {
    for(x1 = 0; x1 < D1; x1++) {
      for(x0 = 0; x0 < D0; x0++) {
        current = get(
          D2, D1, D0,
          x2, x1, x0
        );
        EXPECT_EQ(old + 1, current);
        old = current;
      }
    }
  }
}

TEST(ToolTests, Logarithms)
{
  EXPECT_EQ(floorlog2(1), 0);
  EXPECT_EQ(floorlog2(2), 1); // 2^n
  EXPECT_EQ(floorlog2(3), 1);
  EXPECT_EQ(floorlog2(4), 2); // 2^n
  EXPECT_EQ(floorlog2(5), 2);
  EXPECT_EQ(floorlog2(6), 2);
  EXPECT_EQ(floorlog2(7), 2);
  EXPECT_EQ(floorlog2(8), 3); // 2^n
  EXPECT_EQ(floorlog2(9), 3);
  EXPECT_EQ(floorlog2(10), 3);
  EXPECT_EQ(floorlog2(11), 3);
  EXPECT_EQ(floorlog2(12), 3);
  EXPECT_EQ(floorlog2(13), 3);
  EXPECT_EQ(floorlog2(14), 3);
  EXPECT_EQ(floorlog2(15), 3);
  EXPECT_EQ(floorlog2(16), 4); // 2^n
  EXPECT_EQ(floorlog2(17), 4);

  EXPECT_EQ(ceillog2(1), 0);
  EXPECT_EQ(ceillog2(2), 1); // 2^n
  EXPECT_EQ(ceillog2(3), 2);
  EXPECT_EQ(ceillog2(4), 2); // 2^n
  EXPECT_EQ(ceillog2(5), 3);
  EXPECT_EQ(ceillog2(6), 3);
  EXPECT_EQ(ceillog2(7), 3);
  EXPECT_EQ(ceillog2(8), 3); // 2^n
  EXPECT_EQ(ceillog2(9), 4);
  EXPECT_EQ(ceillog2(10), 4);
  EXPECT_EQ(ceillog2(11), 4);
  EXPECT_EQ(ceillog2(12), 4);
  EXPECT_EQ(ceillog2(13), 4);
  EXPECT_EQ(ceillog2(14), 4);
  EXPECT_EQ(ceillog2(15), 4);
  EXPECT_EQ(ceillog2(16), 4); // 2^n
  EXPECT_EQ(ceillog2(17), 5);

  for(int i = 1; i < 32; i++) {
    EXPECT_EQ(floorlog2((1UL << i) - 1), i - 1);
    EXPECT_EQ(floorlog2((1UL << i) + 0), i);
    EXPECT_EQ(floorlog2((1UL << i) + 1), i);

    if(i == 1)
      EXPECT_EQ(ceillog2((1UL << i) - 1), i - 1);
    else
      EXPECT_EQ(ceillog2((1UL << i) - 1), i);
    EXPECT_EQ(ceillog2((1UL << i) + 0), i);
    EXPECT_EQ(ceillog2((1UL << i) + 1), i + 1);
  }
}
