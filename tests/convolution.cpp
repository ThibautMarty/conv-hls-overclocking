#include <gtest/gtest.h>
#include <stdlib.h>

#include "convolution.h"
#include "io.h"
#include "golden_convolution.h"

namespace
{
  class ConvolutionTest : public ::testing::Test
  {
    protected:
      data_in_t input[BATCHES][N][RR][CC];
      data_in_t weights[BATCHES][N][M][K][K];
      data_in_t output[BATCHES][M][R][C];
      data_in_t golden_output[BATCHES][M][R][C];

      virtual void SetUp()
      {
        srand(42);
        fill_random(input, weights);
      }
  };
} // namespace

TEST_F(ConvolutionTest, OptionsValidness)
{
  EXPECT_EQ(Tn % Un, 0);
  EXPECT_EQ(Tm % Um, 0);

#ifdef OPTIDSP
  EXPECT_GE(Um, OPTIDSPSTEPS);
#endif
}

TEST_F(ConvolutionTest, PrepareInputTile)
{
  data_in_t input_tile[Tn][Trr][Tcc];
  int row, col, ti;
  int iti, ir, ic;

  for(row = 0; row < R; row += Tr)
  {
    for(col = 0; col < C; col += Tc)
    {
      for(ti = 0; ti < N; ti += Tn)
      {
        prepare_input_tile(ti, row, col, input[0], input_tile);

        for(iti = 0; iti < Tn; iti++)
        {
          for(ir = 0; ir < Trr; ir++)
          {
            for(ic = 0; ic < Tcc; ic++)
            {
              // NEEDS to be padded with 0
              if((iti >= N - ti) || (ir >= RR - S * row) || (ic >= CC - S * col))
                EXPECT_EQ(
                  data_in_t(0),
                  input_tile[iti][ir][ic]
                );
              else
                EXPECT_EQ(
                  input[0][ti + iti][S * row + ir][S * col + ic],
                  input_tile[iti][ir][ic]
                );
            }
          }
        }
      }
    }
  }
}

TEST_F(ConvolutionTest, PrepareWeightsTile)
{
  data_in_t weights_tile[Tn][Tm][K][K];
  int to, ti;
  int ito, iti, ir, ic;

  for(to = 0; to < M; to += Tm)
  {
    for(ti = 0; ti < N; ti += Tn)
    {
      prepare_weights_tile(to, ti, weights[0], weights_tile);

      for(iti = 0; iti < Tn; iti++)
      {
        for(ito = 0; ito < Tm; ito++)
        {
          for(ir = 0; ir < K; ir++)
          {
            for(ic = 0; ic < K; ic++)
            {
              // NEEDS to be padded with 0
              if((ito >= M - to) || (iti >= N - ti))
                EXPECT_EQ(
                  data_in_t(0),
                  weights_tile[iti][ito][ir][ic]
                );
              else
                EXPECT_EQ(
                  weights[0][ti + iti][to + ito][ir][ic],
                  weights_tile[iti][ito][ir][ic]
                );
            }
          }
        }
      }
    }
  }
}

TEST_F(ConvolutionTest, ManageOutputTile)
{
  data_in_t output_tile[Tm][Tr][Tc];
  int row, col, to;
  int ito, ir, ic;

  // To test manage_output_tile alone, we initialize `output_tile` with random data
  // and `output` to garbage
  srand(42);

  for(ito = 0; ito < Tm; ito++)
  {
    for(ir = 0; ir < Tr; ir++)
    {
      for(ic = 0; ic < Tc; ic++)
      {
        output_tile[ito][ir][ic] = data_in_t(rand());
      }
    }
  }

  for(to = 0; to < M; to += Tm)
  {
    for(row = 0; row < R; row += Tr)
    {
      for(col = 0; col < C; col += Tc)
      {
        output[0][to][row][col] = rand();
      }
    }
  }

  for(row = 0; row < R; row += Tr)
  {
    for(col = 0; col < C; col += Tc)
    {
      for(to = 0; to < M; to += Tm)
      {
        manage_output_tile(to, row, col, output_tile, output[0]);

        // Check only valid output space
        for(ito = 0; ito < Tm; ito++)
        {
          for(ir = 0; ir < MIN(Tr, R - row); ir++)
          {
            for(ic = 0; ic < MIN(Tc, C - col); ic++)
            {
              EXPECT_EQ(
                output[0][to + ito][row + ir][col + ic],
                output_tile[ito][ir][ic]
              );
            }
          }
        }
      }
    }
  }
}

TEST_F(ConvolutionTest, ABFTValid)
{
  // ABFT is not valid outside of these constraints
  ASSERT_GT(Tcc, 2 * K - S);
  ASSERT_GT(Trr, 2 * K - S);
}

TEST_F(ConvolutionTest, ABFTTotal)
{
  bool failed[TILES];
  int failedcount;
  int tile;

  failedcount = convolution(
    input,
    weights,
    output,
    failed
  );

  // ABFT cannot fail on software
  for(tile = 0; tile < TILES; tile++)
    EXPECT_FALSE(failed[tile]);
  EXPECT_EQ(0, failedcount);
}

TEST_F(ConvolutionTest, GoldenComparison)
{
  int b, ito, ir, ic;
  bool failed[TILES];

  golden_convolution(
    input,
    weights,
    golden_output
  );

  convolution(
    input,
    weights,
    output,
    failed
  );

  // Compare results
  for(b = 0; b < BATCHES; b++)
  {
    for(ito = 0; ito < M; ito++)
    {
      for(ir = 0; ir < R; ir++)
      {
        for(ic = 0; ic < C; ic++)
        {
          EXPECT_EQ(
            output[b][ito][ir][ic],
            golden_output[b][ito][ir][ic]
          );
        }
      }
    }
  }
}

TEST_F(ConvolutionTest, Compatibility)
{
  // Always OK here because we don't use a shared lib for tests,
  // so we cannot mess up with shared lib.
  ASSERT_TRUE(compatibility_check(M, N, R, C, K, S, BATCHES));
  ASSERT_FALSE(compatibility_check(M + 1, N, R, C, K, S, BATCHES));
  ASSERT_FALSE(compatibility_check(M, N + 1, R, C, K, S, BATCHES));
  ASSERT_FALSE(compatibility_check(M, N, R + 1, C, K, S, BATCHES));
  ASSERT_FALSE(compatibility_check(M, N, R, C + 1, K, S, BATCHES));
  ASSERT_FALSE(compatibility_check(M, N, R, C, K + 1, S, BATCHES));
  ASSERT_FALSE(compatibility_check(M, N, R, C, K, S + 1, BATCHES));
  ASSERT_FALSE(compatibility_check(M, N, R, C, K, S, BATCHES + 1));
}
