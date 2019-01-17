#ifndef __CONVOLUTION_H
#define __CONVOLUTION_H

#include "conv_accel.h"
#include "tools.h"
#include "options.sw.h"
#include "golden_convolution.h"

extern "C" {
#include "sds_lib.h"
}

#include <err.h>     // err
#include <cstring> // memcpy
#include <iostream>

// Software wrapper to prepare data
// For now, set failed to true if an error is detected with ABFT.
// Set doabft to false to disable all (software) computations related to ABFT.
// If ENABLE_HARDWARE_ABFT is defined, hardware will still compute ABFT.
// Useful to measure convolution speedup.
// perf_counter arguments are exposed for speedup measurement
// Returns number of failed tiles
int convolution(
  data_in_t input[BATCHES][N][RR][CC],
  data_in_t weights[BATCHES][N][M][K][K],
  data_in_t output[BATCHES][M][R][C],
  bool failed[TILES],
  bool doabft = true,
  perf_counter *intern = NULL,
  perf_counter *abft_sw = NULL
);

// Copy functions
void prepare_input_tile(
  int ti, int row, int col,
  data_in_t input[N][RR][CC],
  data_in_t input_tile[Tn][Trr][Tcc]
);
void prepare_weights_tile(
  int to, int ti,
  data_in_t weights[N][M][K][K],
  data_in_t weights_tile[Tn][Tm][K][K]
);
void manage_output_tile(
  int to, int row, int col,
  data_in_t output_tile[Tm][Tr][Tc],
  data_in_t output[M][R][C]
);

void sw_incs(
  data_in_t input_tile[TILES][TILES_N][Tn][Trr][Tcc],
  data_in_t weights_tile[TILES][TILES_N][Tn][Tm][K][K],
  data_in_t incs[TILES]
);

// Print preprocessors constants
void print_convolution_constants();

// Check for compatibility between executable and shared lib
bool compatibility_check(int m, int n, int r, int c, int k, int s, int b);

#endif // __CONVOLUTION_H
