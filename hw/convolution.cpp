#include "convolution.h"
#include <iostream>

void prepare_input_tile(
  int ti, int row, int col,
  data_in_t input[N][RR][CC],
  data_in_t input_tile[Tn][Trr][Tcc]
)
{
  int iti, ir, ic;

  for(iti = 0; iti < Tn; iti++)
  {
    for(ir = 0; ir < Trr; ir++)
    {
      for(ic = 0; ic < Tcc; ic++)
      {
        // padding
        if((iti >= N - ti) || (ir >= RR - S * row) || (ic >= CC - S * col))
          input_tile[iti][ir][ic] = 0;
        else
          input_tile[iti][ir][ic] = input[ti + iti][S * row + ir][S * col + ic];
      }
    }
  }
}


void prepare_weights_tile(
  int to, int ti,
  data_in_t weights[N][M][K][K],
  data_in_t weights_tile[Tn][Tm][K][K]
)
{
  int ito, iti, ir, ic;

  for(iti = 0; iti < Tn; iti++)
  {
    for(ito = 0; ito < Tm; ito++)
    {
      for(ir = 0; ir < K; ir++)
      {
        for(ic = 0; ic < K; ic++)
        {
          // padding
          if((ito >= M - to) || (iti >= N - ti))
            weights_tile[iti][ito][ir][ic] = 0;
          else
            weights_tile[iti][ito][ir][ic] = weights[ti + iti][to + ito][ir][ic];
        }
      }
    }
  }
}


void manage_output_tile(
  int to, int row, int col,
  data_in_t output_tile[Tm][Tr][Tc],
  data_in_t output[M][R][C]
)
{
  int ito, ir, ic;

  for(ito = 0; ito < Tm; ito++)
  {
    for(ir = 0; ir < MIN(Tr, R - row); ir++)
    {
      for(ic = 0; ic < MIN(Tc, C - col); ic++)
      {
        output[to + ito][row + ir][col + ic] =
          output_tile[ito][ir][ic];
      }
    }
  }
}

int convolution(
  data_in_t input[BATCHES][N][RR][CC],
  data_in_t weights[BATCHES][N][M][K][K],
  data_in_t output[BATCHES][M][R][C],
  bool failed[TILES],
  bool doabft,
  perf_counter *intern,
  perf_counter *abft_sw
)
{
  // Tile loops indexes
  int b, row, col, to, ti, tile;
  int row1, col1, to1, ti1; // row = row1 * Tr

  // Tiles (allocated on heap)
  data_in_t (*input_tile)[TILES_N][Tn][Trr][Tcc];
  data_in_t (*weights_tile)[TILES_N][Tn][Tm][K][K];
  data_in_t (*output_tile)[Tm][Tr][Tc];

  int failedcount = 0;

  // ABFT
#ifdef ENABLE_HARDWARE_ABFT
  ap_uint<FAILED_BITS> failed_tile[FAILED_SIZE];
#else
  data_in_t incs[TILES];
  data_out_t outcs;
  int ito, ir, ic;
#endif

  input_tile =
    (data_in_t (*) [TILES_N][Tn][Trr][Tcc]) sds_alloc(
    TILES * TILES_N * Tn * Trr * Tcc *
    sizeof(data_in_t)
  );
  weights_tile =
    (data_in_t (*) [TILES_N][Tn][Tm][K][K]) sds_alloc(
    TILES * TILES_N * Tn * Tm * K * K *
    sizeof(data_in_t)
  );
  output_tile =
    (data_in_t (*) [Tm][Tr][Tc]) sds_alloc(
    TILES * Tm * Tr * Tc *
    sizeof(data_in_t)
  );

  if((input_tile == NULL) ||
    (weights_tile == NULL) ||
    (output_tile == NULL))
  {
    err(-2, "memory allocation error");
  }

  // Prepare data
  tile = 0;
  for(b = 0; b < BATCHES; b++)
  {
    for(to = 0, to1 = 0; to < M; to += Tm, to1++)
    {
      for(row = 0, row1 = 0; row < R; row += Tr, row1++)
      {
        for(col = 0, col1 = 0; col < C; col += Tc, col1++)
        {
          for(ti = 0, ti1 = 0; ti < N; ti += Tn, ti1++)
          {
            prepare_input_tile(
              ti, row, col,
              input[b],
              input_tile[tile][ti1]
            );

            // Note that we could only send TILES_M * TILES_N weights tiles
            // by interchanging loops row/col and ti in conv_compute_tile.
            // But in this case we would need to receive more output tiles
            // (TILES_N more). TILES_R and TILES_C are likely to be 1 so this
            // solution is better
            prepare_weights_tile(
              to, ti,
              weights[b],
              weights_tile[tile][ti1]
            );
          } // ti

          tile++;
        } // col
      } // row
    } // to
  } // b


  if(intern != NULL)
    intern->start();

  // If we compute ABFT by software, compute input-checksum in parallel
#ifndef ENABLE_HARDWARE_ABFT
#pragma SDS async(1)
#endif
  hw_toplevel(
    input_tile,
    weights_tile,
    output_tile
#ifdef ENABLE_HARDWARE_ABFT
    , failed_tile
#endif
  );

#ifdef ENABLE_HARDWARE_ABFT
  if(intern != NULL)
    intern->stop();
#else
  if(doabft)
  {
    if(abft_sw != NULL)
      abft_sw->start();

    sw_incs(
      input_tile,
      weights_tile,
      incs
    );

    if(abft_sw != NULL)
      abft_sw->stop();
  }

  // Wait for hardware call end
#pragma SDS wait(1)
  if(intern != NULL)
    intern->stop();

#endif

  if(doabft)
  {
#ifdef ENABLE_HARDWARE_ABFT
    for(tile = 0; tile < TILES; tile++)
    {
      failed[tile] = (failed_tile[tile / FAILED_BITS][tile % FAILED_BITS] != 0);
      if(failed[tile])
        failedcount++;
    }
#else
    for(tile = 0; tile < TILES; tile++)
    {
      outcs = 0;

      for(ito = 0; ito < Tm; ito++)
      {
        for(ir = 0; ir < Tr; ir++)
        {
          for(ic = 0; ic < Tc; ic++)
          {
            outcs += output_tile[tile][ito][ir][ic];
          }
        }
      }
      failed[tile] = (incs[tile] != data_in_t(outcs));
      if(failed[tile])
        failedcount++;
    }
#endif
  }

  // manage output tile
  tile = 0;
  for(b = 0; b < BATCHES; b++)
  {
    for(to = 0, to1 = 0; to < M; to += Tm, to1++)
    {
      for(row = 0, row1 = 0; row < R; row += Tr, row1++)
      {
        for(col = 0, col1 = 0; col < C; col += Tc, col1++)
        {
          manage_output_tile(
            to, row, col,
            output_tile[tile],
            output[b]
          );
          tile++;
        } // col
      } // row
    } // to
  } // b

  sds_free(input_tile);
  sds_free(weights_tile);
  sds_free(output_tile);

  return failedcount;
} // convolution()

#ifndef ENABLE_HARDWARE_ABFT

void sw_incs(
  data_in_t input_tile[TILES][TILES_N][Tn][Trr][Tcc],
  data_in_t weights_tile[TILES][TILES_N][Tn][Tm][K][K],
  data_in_t incs[TILES]
)
{
  // Note: we cannot perform an accurate output checksum with accelerator's output (in data_in_t format, not data_out_t).
  // So here we just compute the checksum by computing again convolution, not with ABFT techniques.

  data_out_t tmp[Tm][Tr][Tc];
  data_out_t incs_tmp;

  // Loop indexes
  int i, j, ir, ic, ito, iti, ti1, tile;

  for(tile = 0; tile < TILES; tile++)
  {
    incs_tmp = 0;
    for(ir = 0; ir < Tr; ir++)
    {
      for(ic = 0; ic < Tc; ic++)
      {
        for(ito = 0; ito < Tm; ito++)
        {
          tmp[ito][ir][ic] = 0;
        }
      }
    }

    for(ti1 = 0; ti1 < TILES_N; ti1++)
    {
      for(ir = 0; ir < Tr; ir++)
      {
        for(ic = 0; ic < Tc; ic++)
        {
          for(ito = 0; ito < Tm; ito++)
          {
            for(i = 0; i < K; i++)
            {
              for(j = 0; j < K; j++)
              {
                for(iti = 0; iti < Tn; iti++)
                {
                  tmp[ito][ir][ic] +=
                    weights_tile[tile][ti1][iti][ito][i][j] *
                    input_tile[tile][ti1][iti][S * ir + i][S * ic + j];
                } // iti
              } // j
            } // i
          } // ito
        } // ic
      } // ir
    } // ti1

    for(ir = 0; ir < Tr; ir++)
    {
      for(ic = 0; ic < Tc; ic++)
      {
        for(ito = 0; ito < Tm; ito++)
        {
          incs_tmp += (tmp[ito][ir][ic] >> (data_in_t::width - 1));
        }
      }
    }

    incs[tile] = data_in_t(incs_tmp);
  }
} // sw_incs()

#else

#define S1init(n, sy, sx) section[sy][sx] = 0
#define S1UL(n, sy, sx, iy, ix) section[sy][sx] += input[n][iy][ix]
#define S1DL(n, sy, sx, iy, ix) S1UL(n, sy, sx, iy, ix)
#define S1UR(n, sy, sx, iy, ix) S1UL(n, sy, sx, iy, ix)
#define S1DR(n, sy, sx, iy, ix) S1UL(n, sy, sx, iy, ix)
#define S1top(n, sy, sx, iy, ix) S1UL(n, sy, sx, iy, ix)
#define S1bottom(n, sy, sx, iy, ix) S1UL(n, sy, sx, iy, ix)
#define S1left(n, sy, sx, iy, ix) S1UL(n, sy, sx, iy, ix)
#define S1right(n, sy, sx, iy, ix) S1UL(n, sy, sx, iy, ix)
#define S1center(n, sy, sx, iy, ix) S1UL(n, sy, sx, iy, ix)
#define X1initfirst(n) X[ 0][ 0] = 0
#define X1first(n, sy, sx) X[ 0][ 0] += section[sy][sx]
#define X1initcolumn(n, xy) X[xy][ 0] = X[xy - 1][ 0]
#define X1column(n, xy, sx) X[xy][ 0] += section[xy + K - 1][sx] - section[xy - 1][sx]
#define X1initline(n, xy, xx) X[xy][xx] = X[xy][xx - 1]
#define X1line(n, xy, xx, sy) X[xy][xx] += section[sy][xx + K - 1] - section[sy][xx - 1]
#define K1init(n, ky, kx) kernel[ky][kx] = 0
#define K1(m, n, ky, kx) kernel[ky][kx] += weight[n][m][ky][kx]
#define R1(n, y, x) rho += X[y][x] * kernel[y][x]

void compute_incs(
  data_in_t input[Tn][Trr][Tcc],
  data_in_t weight[Tn][Tm][K][K],
  data_in_t *incs,
  bool start,
  bool end
){
  section_t section[2 * K - 1][2 * K - 1];
  kernel_t kernel[K][K];
  ap_int<data_in_t::width + ceillog2(Tr * Tc) + ceillog2(K * K)> X[K][K];
  static data_checksum_t rho; // max 32 bits

  if(start)
    rho = 0;

  for (int c0 = 0; c0 < Tn; c0 += 1) {
    for (int c5 = 0; c5 < 2 * K - 1; c5 += 1)
      for (int c6 = 0; c6 < 2 * K - 1; c6 += 1)
        S1init(c0, c5, c6);
    for (int c3 = 0; c3 < K - 1; c3 += 1) {
      for (int c4 = 0; c4 < K - 1; c4 += 1)
        S1UL(c0, c3, c4, c3, c4);
      for (int c4 = K - 1; c4 <= Tcc - K; c4 += 1)
        S1top(c0, c3, K - 1, c3, c4);
      for (int c4 = Tcc - K + 1; c4 < Tcc; c4 += 1)
        S1UR(c0, c3, -Tcc + 2 * K + c4 - 1, c3, c4);
    }
    for (int c3 = K - 1; c3 <= Trr - K; c3 += 1) {
      for (int c4 = 0; c4 < K - 1; c4 += 1)
        S1left(c0, K - 1, c4, c3, c4);
      for (int c4 = K - 1; c4 <= Tcc - K; c4 += 1)
        S1center(c0, K - 1, K - 1, c3, c4);
      for (int c4 = Tcc - K + 1; c4 < Tcc; c4 += 1)
        S1right(c0, K - 1, -Tcc + 2 * K + c4 - 1, c3, c4);
    }
    for (int c3 = Trr - K + 1; c3 < Trr; c3 += 1) {
      for (int c4 = 0; c4 < K - 1; c4 += 1)
        S1DL(c0, -Trr + 2 * K + c3 - 1, c4, c3, c4);
      for (int c4 = K - 1; c4 <= Tcc - K; c4 += 1)
        S1bottom(c0, -Trr + 2 * K + c3 - 1, K - 1, c3, c4);
      for (int c4 = Tcc - K + 1; c4 < Tcc; c4 += 1)
        S1DR(c0, -Trr + 2 * K + c3 - 1, -Tcc + 2 * K + c4 - 1, c3, c4);
    }
    X1initfirst(c0);
    for (int c3 = 0; c3 < K; c3 += 1)
      for (int c4 = 0; c4 < K; c4 += 1)
        X1first(c0, c3, c4);
    for (int c2 = 1; c2 < K; c2 += 1) {
      X1initcolumn(c0, c2);
      for (int c4 = 0; c4 < K; c4 += 1)
        X1column(c0, c2, c4);
    }
    for (int c2 = 0; c2 < K; c2 += 1)
      for (int c3 = 1; c3 < K; c3 += 1) {
        X1initline(c0, c2, c3);
        for (int c5 = c2; c5 < K + c2; c5 += 1)
          X1line(c0, c2, c3, c5);
      }
    for (int c4 = 0; c4 < K; c4 += 1)
      for (int c5 = 0; c5 < K; c5 += 1)
        K1init(c0, c4, c5);
    for (int c3 = 0; c3 < Tm; c3 += 1)
      for (int c4 = 0; c4 < K; c4 += 1)
        for (int c5 = 0; c5 < K; c5 += 1)
          K1(c3, c0, c4, c5);
    for (int c4 = 0; c4 < K; c4 += 1)
      for (int c5 = 0; c5 < K; c5 += 1)
        R1(c0, c4, c5);
  }

  if(end)
    *incs = data_in_t(rho >> (data_in_t::width - 1));
} // compute_incs()

/*
void sw_incs(
  data_in_t input_tile[TILES][TILES_N][Tn][Trr][Tcc],
  data_in_t weights_tile[TILES][TILES_N][Tn][Tm][K][K],
  data_in_t incs[TILES]
)
{
  int tile, ti, ti1;
  for(tile = 0; tile < TILES; tile++)
  {
    for(ti = 0, ti1 = 0; ti < N; ti += Tn, ti1++)
    {
      compute_incs(
        input_tile[tile][ti1],
        weights_tile[tile][ti1],
        &(incs[tile]),
        ti1 == 0,
        ti1 == TILES_N - 1
      );
    } // ti
  } // tile
} // sw_incs()
*/

#endif

void print_convolution_constants()
{
  std::cerr <<
    "N "  << N << std::endl <<
    "M "  << M << std::endl <<
    "R "  << R << std::endl <<
    "C "  << C << std::endl <<
    "Tm " << Tm << std::endl <<
    "Tn " << Tn << std::endl <<
    "Um " << Um << std::endl <<
    "Un " << Un << std::endl <<
    "Tr " << Tr << std::endl <<
    "Tc " << Tc << std::endl <<
    "K "  << K << std::endl <<
    "S "  << S << std::endl <<
    "platform " << TOSTRING(PLATFORM) << std::endl <<
    "DATA_WL " << DATA_WL << std::endl <<
    "INPUT_CLK: " << INPUT_CLK << std::endl <<
    "SAFE_CLK: " << SAFE_CLK << std::endl <<
#ifdef ENABLE_HARDWARE_ABFT
    "enable_hardware_abft 1" << std::endl
#else
    "enable_hardware_abft 0" << std::endl
#endif
  ;
}

bool compatibility_check(int m, int n, int r, int c, int k, int s, int b)
{
  return
    (M == m) &&
    (N == n) &&
    (R == r) &&
    (C == c) &&
    (K == k) &&
    (S == s) &&
    (BATCHES == b);
}
