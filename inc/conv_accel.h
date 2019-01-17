#ifndef __CONV_ACCEL_H
#define __CONV_ACCEL_H

#include "options.h"
#include "tools.h"
#include <hls_stream.h>

typedef ap_int<data_in_t::width + ceillog2(Tr * Tc)> section_t;
typedef ap_int<data_in_t::width + ceillog2(Tm)> kernel_t;

// Computed sizes from options
#define Trr (((Tr - 1) * S) + K)
#define Tcc (((Tc - 1) * S) + K)
#define RR  (((R  - 1) * S) + K)
#define CC  (((C  - 1) * S) + K)
#define TILES_N (UPPERDIV(N, Tn))
#define TILES_M (UPPERDIV(M, Tm))
#define TILES_R (UPPERDIV(R, Tr))
#define TILES_C (UPPERDIV(C, Tc))

#define TILES (BATCHES * TILES_M * TILES_R * TILES_C)

// Tradeoff between big array/bit wordlength
#define FAILED_BITS 32
#define FAILED_SIZE (UPPERDIV(TILES, 32))

// Accelerator top-level: computes a convolution tile & abft
void hw_toplevel(
  // Inputs
  data_in_t input_tile[TILES][TILES_N][Tn][Trr][Tcc],
  data_in_t weights_tile[TILES][TILES_N][Tn][Tm][K][K],

  // Outputs
  data_in_t output_tile[TILES][Tm][Tr][Tc]

#ifdef ENABLE_HARDWARE_ABFT
  , ap_uint<FAILED_BITS> failed[FAILED_SIZE]
#endif
);

// Dataflow actors
void hw_recv_input(
  data_in_t input_tile[Tn][Trr][Tcc],
  data_in_t input_tile_hw[Tn][Trr][Tcc]
#ifdef ENABLE_HARDWARE_ABFT
  , hls::stream<section_t>& section_fifo
#endif
);
void hw_recv_weights(
  data_in_t weights_tile[Tn][Tm][K][K],
  data_in_t weights_tile_hw[Tn][Tm][K][K]
#ifdef ENABLE_HARDWARE_ABFT
  , hls::stream<kernel_t>& kernel_fifo
#endif
);
void hw_send_output(
  bool end,
  hls::stream<data_in_t> output_fifo[Um],
  data_in_t output_tile[Tm][Tr][Tc]
);
void hw_conv(
  bool start, bool end,
  ap_uint<1> pingpong,
  data_in_t input_tile_hw[Tn][Trr][Tcc],
  data_in_t weights_tile_hw[Tn][Tm][K][K],
#ifdef ENABLE_HARDWARE_ABFT
  hls::stream<data_out_t> output_fifo_fullp[Um]
#else
  hls::stream<data_in_t> output_fifo[Um]
#endif
);
#ifdef ENABLE_HARDWARE_ABFT
void hw_incs(
  bool start, bool end,
  hls::stream<section_t>& section_fifo,
  hls::stream<kernel_t>& kernel_fifo,
  data_in_t *incs
);
void hw_outcs(
  bool end,
  hls::stream<data_out_t> output_fifo_fullp[Um],
  hls::stream<data_in_t> output_fifo[Um],
  data_in_t *outcs
);
void hw_compare_cs(
  bool end,
  data_in_t incs,
  data_in_t outcs,
  ap_uint<ceillog2(TILES)> tile,
  ap_uint<FAILED_BITS> failed[FAILED_SIZE]
);
#endif

#endif // __CONV_ACCEL_H
