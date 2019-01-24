#include "conv_accel.h"

void hw_recv_input(
  data_in_t input_tile[Tn][Trr][Tcc],
  data_in_t input_tile_hw[Tn][Trr][Tcc]
#ifdef ENABLE_HARDWARE_ABFT
  , hls::stream<section_t>& section_fifo
#endif
)
{
#ifndef ENABLE_HARDWARE_ABFT

  for(int iti = 0; iti < Tn; iti++)
  {
    for(int ir = 0; ir < Trr; ir++)
    {
      for(int ic = 0; ic < Tcc; ic++)
      {
#pragma HLS PIPELINE rewind
        input_tile_hw[iti][ir][ic] = input_tile[iti][ir][ic];
      }
    }
  }

#else
  // Section coordinates + aging (init to 1, sy/sx will start with 0/0)
  ap_uint<ceillog2(2 * K - 1)> sy, sx, old_sx(1), old_sy(1);

  section_t section[2 * K - 1][2 * K - 1], acc;
#pragma HLS RESOURCE variable=section core=RAM_S2P_LUTRAM


  for(int iti = 0; iti < Tn; iti++)
  {
    for(int ir = 0; ir < Trr; ir++)
    {
      for(int ic = 0; ic < Tcc; ic++)
      {
#pragma HLS PIPELINE rewind

        // False dependency on section: trick with old_sy/sx + acc
#pragma HLS dependence variable=section inter false

        data_in_t input = input_tile[iti][ir][ic];
        input_tile_hw[iti][ir][ic] = input;

        // Compute to which section this input will be accumulated
        if(ir < K - 1)
          sy = ir;
        else if(ir <= Trr - K)
          sy = K - 1;
        else
          sy = -Trr + 2 * K + ir - 1;

        if(ic < K - 1)
          sx = ic;
        else if(ic <= Tcc - K)
          sx = K - 1;
        else
          sx = -Tcc + 2 * K + ic - 1;

        // Accumulate to sections
        if(old_sx == sx && old_sy == sy)
          acc += input;
        else {
          // We need to initialize section if input is its first value accumulated
          if(((ir < K) || (ir > Tr - 1)) && ((ic < K) || (ic > Tc - 1)))
            acc = input;
          else
            acc = section[sy][sx] + input;
        }

        old_sx = sx;
        old_sy = sy;

        // Last step
        if(((ir < K - 1) || (ir > Tr - 2)) && ((ic < K - 1) || (ic > Tc - 2)))
          section_fifo << acc;
        else
          section[sy][sx] = acc;
      }
    }
  }

#endif
} // hw_recv_input()

void hw_recv_weights(
  data_in_t weights_tile[Tn][Tm][K][K],
  data_in_t weights_tile_hw[Tn][Tm][K][K]
#ifdef ENABLE_HARDWARE_ABFT
  , hls::stream<kernel_t>& kernel_fifo
#endif
)
{
#ifndef ENABLE_HARDWARE_ABFT

  for(int iti = 0; iti < Tn; iti++)
  {
    for(int ito = 0; ito < Tm; ito++)
    {
      for(int ir = 0; ir < K; ir++)
      {
        for(int ic = 0; ic < K; ic++)
        {
#pragma HLS PIPELINE rewind
          weights_tile_hw[iti][ito][ir][ic] = weights_tile[iti][ito][ir][ic];
        }
      }
    }
  }

#else

  // Local data
  static kernel_t kernel[K][K];
#pragma HLS RESOURCE variable=kernel core=RAM_S2P_LUTRAM

  for(int iti = 0; iti < Tn; iti++)
  {
    for(int ito = 0; ito < Tm; ito++)
    {
      for(int ir = 0; ir < K; ir++)
      {
        for(int ic = 0; ic < K; ic++)
        {
#pragma HLS PIPELINE rewind

          // False dependency on kernel: ic changes at each cycle
#pragma HLS dependence variable=kernel inter false

          data_in_t weight = weights_tile[iti][ito][ir][ic];
          weights_tile_hw[iti][ito][ir][ic] = weight;

          // First step
          if(ito == 0)
            kernel[ir][ic] = weight;
          // Other steps
          else
            kernel[ir][ic] += weight;

          // Last step
          if(ito == Tm - 1)
            kernel_fifo << kernel[ir][ic];
        }
      }
    }
  }

#endif
} // hw_recv_weights()

void hw_send_output(
  bool end,
  hls::stream<data_in_t> output_fifo[Um],
  data_in_t output_tile[Tm][Tr][Tc]
)
{
  if(end)
  {
    for(int ito1 = 0; ito1 < Tm / Um; ito1++)
    {
      for(int um = 0; um < Um; um++)
      {
        for(int ir = 0; ir < Tr; ir++)
        {
          for(int ic = 0; ic < Tc; ic++)
          {
#pragma HLS PIPELINE
            output_tile[Um * ito1 + um][ir][ic] = output_fifo[um].read();
          }
        }
      }
    }
  }
} // hw_send_output()

#pragma SDS data access_pattern(input_tile:SEQUENTIAL)
#pragma SDS data access_pattern(weights_tile:SEQUENTIAL)
#pragma SDS data access_pattern(output_tile:SEQUENTIAL)
#pragma SDS data access_pattern(failed:SEQUENTIAL)
// #pragma SDS data mem_attribute(input_tile:PHYSICAL_CONTIGUOUS) // Faster in AXIDMA_SIMPLE
#pragma SDS data mem_attribute(weights_tile:PHYSICAL_CONTIGUOUS)
#pragma SDS data mem_attribute(output_tile:PHYSICAL_CONTIGUOUS)
// #pragma SDS data mem_attribute(failed:PHYSICAL_CONTIGUOUS) // Faster without it
void hw_toplevel(
  // Inputs
  data_in_t input_tile[TILES][TILES_N][Tn][Trr][Tcc],
  data_in_t weights_tile[TILES][TILES_N][Tn][Tm][K][K],

  // Outputs
  data_in_t output_tile[TILES][Tm][Tr][Tc]

#ifdef ENABLE_HARDWARE_ABFT
  , ap_uint<FAILED_BITS> failed[FAILED_SIZE]
#endif
)
{
  dataflowTile:for(int tile = 0; tile < TILES; tile++)
  {
    dataflowN:for(int ti1 = 0; ti1 < TILES_N; ti1++)
    {
#pragma HLS DATAFLOW
      hls::stream<data_in_t> output_fifo[Um];
DO_PRAGMA(HLS stream depth=OUTPUT_FIFO_DEPTH variable=output_fifo)

      data_in_t input_tile_hw[Tn][Trr][Tcc];
      data_in_t weights_tile_hw[Tn][Tm][K][K];

#ifdef ENABLE_HARDWARE_ABFT
      hls::stream<data_out_t> output_fifo_fullp[Um];
      hls::stream<section_t> section_fifo;
      hls::stream<kernel_t> kernel_fifo;
DO_PRAGMA(HLS stream depth=K2 variable=kernel_fifo)
/* #pragma HLS RESOURCE variable=output_fifo_fullp core=FIFO_LUTRAM */
/* #pragma HLS RESOURCE variable=section_fifo core=FIFO_LUTRAM */
/* #pragma HLS RESOURCE variable=kernel_fifo core=FIFO_LUTRAM */
#endif

      // Partition for unrolled loop related dimensions
DO_PRAGMA(HLS ARRAY_PARTITION variable=input_tile_hw cyclic factor=Un dim=1)
DO_PRAGMA(HLS ARRAY_PARTITION variable=weights_tile_hw cyclic factor=Un dim=1)
DO_PRAGMA(HLS ARRAY_PARTITION variable=weights_tile_hw cyclic factor=Um dim=2)

      // Force tile memories to be in BRAM (otherwise, in some case, they are
      // implemented in LUTRAM and use too much LUT)
#pragma HLS RESOURCE variable=input_tile_hw core=RAM_2P_BRAM
#pragma HLS RESOURCE variable=weights_tile_hw core=RAM_2P_BRAM

#ifdef ENABLE_HARDWARE_ABFT
      data_in_t incs, outcs;
#endif

      bool start = (ti1 == 0);
      bool end = (ti1 == TILES_N - 1);
      ap_uint<ceillog2(TILES)> tile_df = tile;
      ap_uint<1> pingpong = (ti1 % 2);

      hw_recv_input(
        input_tile[tile][ti1],
        input_tile_hw
#ifdef ENABLE_HARDWARE_ABFT
        , section_fifo
#endif
      );

      hw_recv_weights(
        weights_tile[tile][ti1],
        weights_tile_hw
#ifdef ENABLE_HARDWARE_ABFT
        , kernel_fifo
#endif
      );

#ifdef ENABLE_HARDWARE_ABFT
      hw_incs(
        start,
        end,
        section_fifo,
        kernel_fifo,
        &incs
      );
#endif

      hw_conv(
        start,
        end,
        pingpong,
        input_tile_hw,
        weights_tile_hw,
#ifdef ENABLE_HARDWARE_ABFT
        output_fifo_fullp
#else
        output_fifo
#endif
      );

#ifdef ENABLE_HARDWARE_ABFT
      hw_outcs(
        end,
        output_fifo_fullp,
        output_fifo,
        &outcs
      );
#endif

      hw_send_output(
        end,
        output_fifo,
        output_tile[tile]
      );

#ifdef ENABLE_HARDWARE_ABFT
      hw_compare_cs(
        end,
        incs,
        outcs,
        tile_df,
        failed
      );
#endif

    }
  }
}

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
)
{
  // Local data
  static data_pe_t pe_hw[Um];
  // Note: we are doing double buffering here to avoid II=2 because of output_tile_hw_local.
  // There must be a better (cheaper) solution
  static data_out_t output_tile_hw_local[2][UPPERDIV(Tm, Um)][Um][Tr][Tc];
#pragma HLS ARRAY_PARTITION variable=pe_hw complete
/* #pragma HLS RESOURCE variable=pe_hw core=RAM_S2P_LUTRAM */
DO_PRAGMA(HLS ARRAY_PARTITION variable=output_tile_hw_local complete dim=1)
DO_PRAGMA(HLS ARRAY_PARTITION variable=output_tile_hw_local complete dim=3)
#pragma HLS RESOURCE variable=output_tile_hw_local core=RAM_2P_BRAM

  // Walk on output tile (Tr, Tc, Tm parallelized with factor Um)
  for(int ito = 0, ito1 = 0; ito < Tm; ito += Um, ito1++)
  {
    for(int ir = 0; ir < Tr; ir++)
    {
      for(int ic = 0; ic < Tc; ic++)
      {
        // Kernel loops
        kernel:for(int i = 0; i < K; i++)
        {
          for(int j = 0; j < K; j++)
          {
#if Tn != Un
            for(int iti = 0, iti1 = 0; iti < Tn; iti += Un, iti1++)
            // Walk in input tile (Tn parallelized with factor Un)
#else
            // If Tn == Un, there is no loop. Pipelining is applied
            // to parent loop (j).
            int iti1 = 0, iti = 0;
#endif
            {
#pragma HLS PIPELINE rewind
              // Um Processing Elements, with mul + add tree Un-wide
#ifdef OPTIDSP
#warning "DSP usage optimization enabled"
              for(int um = 0; um < Um; um += OPTIDSPSTEPS)
#else
              for(int um = 0; um < Um; um++)
#endif
              {
#pragma HLS UNROLL

                for(int un = 0; un < Un; un++)
                {
#pragma HLS UNROLL
#ifdef OPTIDSP
                  // Perform several multiplications in 1
                  data_in_t i0 = input_tile_hw[iti + un][S * ir + i][S * ic + j];
                  ap_uint<data_in_t::width> i0abs = i0.sign() ? ap_uint<data_in_t::width>(-i0) : ap_uint<data_in_t::width>(i0);

                  ap_uint<data_in_t::width * (OPTIDSPSTEPS * 2 - 1)> x = 0; // max. 24

                  ap_uint<1> w0sign[OPTIDSPSTEPS];

                  for(int s = 0; (s < OPTIDSPSTEPS) && (um + s < Um); s++)
                  {
#pragma HLS UNROLL
                    data_in_t tmp = weights_tile_hw[iti + un][ito + um + s][i][j];
                    w0sign[s] = tmp.sign();
                    ap_uint<data_in_t::width> tmpabs = w0sign[s] ? ap_uint<data_in_t::width>(-tmp) : ap_uint<data_in_t::width>(tmp);

                    // Prepare operand with absolute values
                    x.range(data_in_t::width * (2 * s + 1) - 1, data_in_t::width * 2 * s) = ap_uint<data_in_t::width * 2>(tmpabs);
                  }

                  // Product
                  ap_uint<data_in_t::width * (OPTIDSPSTEPS * 2)> pe_prod = x * i0abs;

                  // Extract results + correct sign + accumulate
                  for(int s = 0; (s < OPTIDSPSTEPS) && (um + s < Um); s++)
                  {
#pragma HLS UNROLL
                    ap_int<2 * data_in_t::width> pe_prod0 =
                      (w0sign[s] ^ i0.sign()) ?
                        ap_int<data_in_t::width * 2>(-(pe_prod.range(data_in_t::width * 2 * (s + 1) - 1, data_in_t::width * 2 * s))) :
                        ap_int<data_in_t::width * 2>(pe_prod.range(data_in_t::width * 2 * (s + 1) - 1, data_in_t::width * 2 * s));

                    data_pe_t pe_sum0 = pe_hw[um + s] + pe_prod0;

                    if((i == 0) && (j == 0) && (iti1 == 0) && (un == 0))
                      pe_hw[um + s] = pe_prod0;
                    else
                      pe_hw[um + s] = pe_sum0;
                  }
#else // OPTIDSP
                  data_pe_t pe_prod =
                    weights_tile_hw[iti + un][ito + um][i][j] *
                    input_tile_hw[iti + un][S * ir + i][S * ic + j];

#ifdef NODSP
#pragma HLS RESOURCE variable=pe_prod core=Mul_LUT
#endif

                  data_pe_t pe_sum = pe_hw[um] + pe_prod;

                  if((i == 0) && (j == 0) && (iti1 == 0) && (un == 0))
                    pe_hw[um] = pe_prod;
                  else
                    pe_hw[um] = pe_sum;
#endif // OPTIDSP
                } // un

                // Last iteration: send output to next actor
                if((iti == Tn - Un) && (j == K - 1) && (i == K - 1))
                {
#pragma HLS dependence variable=output_tile_hw_local inter false
#ifdef OPTIDSP
                  for(int s = 0; (s < OPTIDSPSTEPS) && (um + s < Um); s++)
                  {
#pragma HLS UNROLL
                    // Accumulate to output_tile_hw_local
                    data_pe_t pe_output_sum = output_tile_hw_local[1 - pingpong][ito1][um + s][ir][ic] + pe_hw[um + s];
                    if(start)
                      output_tile_hw_local[pingpong][ito1][um + s][ir][ic] = pe_hw[um + s];
                    else if(end)
#ifdef ENABLE_HARDWARE_ABFT
                      output_fifo_fullp[um + s] << pe_output_sum;
#else
                      output_fifo[um + s] << (pe_output_sum >> (data_in_t::width - 1));
#endif
                    else
                      output_tile_hw_local[pingpong][ito1][um + s][ir][ic] = pe_output_sum;
                  }
#else // OPTIDSP
                  // Accumulate to output_tile_hw_local
                  data_pe_t pe_output_sum = output_tile_hw_local[1 - pingpong][ito1][um][ir][ic] + pe_hw[um];
                  if(start)
                    output_tile_hw_local[pingpong][ito1][um][ir][ic] = pe_hw[um];
                  else if(end)
#ifdef ENABLE_HARDWARE_ABFT
                    output_fifo_fullp[um] << pe_output_sum;
#else
                    output_fifo[um] << (pe_output_sum >> (data_in_t::width - 1));
#endif
                  else
                    output_tile_hw_local[pingpong][ito1][um][ir][ic] = pe_output_sum;
#endif // OPTIDSP
                }
              } // um
            } // iti
          } // j
        } // i
      } // ic
    } // ir
  } // ito
} // hw_conv()


#ifdef ENABLE_HARDWARE_ABFT

void hw_incs(
  bool start, bool end,
  hls::stream<section_t>& section_fifo,
  hls::stream<kernel_t>& kernel_fifo,
  data_in_t *incs
)
{
  // Local data
  static section_t section[2 * K - 1][2 * K - 1];
  static ap_int<data_in_t::width + ceillog2(Tr * Tc) + ceillog2(K * K)> X[K][K];
  static data_checksum_t rho; // max 32 bits

  // Do not use BRAM for these arrays
/* #pragma HLS RESOURCE variable=section core=RAM_S2P_LUTRAM */
/* #pragma HLS RESOURCE variable=X core=RAM_S2P_LUTRAM // Note: this one could be 1P, but S2P uses less LUT (more FF) */

DO_PRAGMA(HLS ARRAY_PARTITION variable=X cyclic factor=2 dim=1)
DO_PRAGMA(HLS ARRAY_PARTITION variable=X cyclic factor=2 dim=2)

  incs_Nloop:for(int iti = 0; iti < Tn; iti++)
  {
    incs_sectioncopy:for(int sy = 0; sy < 2 * K - 1; sy++) {
      for(int sx = 0; sx < 2 * K - 1; sx++) {
#pragma HLS PIPELINE
        section_fifo >> section[sy][sx];

        // init for incsX00 loop
        X[0][0] = 0;
      }
    }


    // Compute X[0][0]
    incsX00:for(int c3 = 0; c3 < K; c3 += 1) {
      for(int c4 = 0; c4 < K; c4 += 1) {
#pragma HLS PIPELINE
        X[0][0] += section[c3][c4];
      }
    }

    // Compute X[*][0]
    incsXn0:for(int c2 = 1; c2 < K; c2 += 1) {
      for(int c4 = 0; c4 < K; c4 += 1) {
#pragma HLS PIPELINE
        if(c4 == 0)
          X[c2][0] = X[c2 - 1][0] + section[c2 + K - 1][c4] - section[c2 - 1][c4];
        else
          X[c2][0] += section[c2 + K - 1][c4] - section[c2 - 1][c4];
      }
    }

    // Compute X[*][*]
    incsXnn:for(int c2 = 0; c2 < K; c2 += 1) {
      for(int c3 = 1; c3 < K; c3 += 1) {
        for(int c5 = 0; c5 < K; c5 += 1) {
#pragma HLS PIPELINE
          if(c5 == 0)
            X[c2][c3] = X[c2][c3 - 1] + section[c5 + c2][c3 + K - 1] - section[c5 + c2][c3 - 1];
          else
            X[c2][c3] += section[c5 + c2][c3 + K - 1] - section[c5 + c2][c3 - 1];
        }
      }
    }

    // Compute product + final accumulation
    incsacc:for(int c4 = 0; c4 < K; c4 += 1) {
      for(int c5 = 0; c5 < K; c5 += 1) {
#pragma HLS PIPELINE
        data_checksum_t prod = kernel_fifo.read() * X[c4][c5];
#pragma HLS RESOURCE variable=prod core=Mul_LUT

        rho += prod;
      }
    }
  } // iti

  if(end) {
    *incs = data_in_t(rho >> (data_in_t::width - 1));
    rho = 0; // Reset for next call
  }
} // hw_incs()

void hw_outcs(
  bool end,
  hls::stream<data_out_t> output_fifo_fullp[Um],
  hls::stream<data_in_t> output_fifo[Um],
  data_in_t *outcs
)
{
  static data_out_t outcs_hw[Um];
#pragma HLS ARRAY_PARTITION variable=outcs_hw complete

  if(end) {
    for(int ito = 0, ito1 = 0; ito < Tm; ito += Um, ito1++)
    {
      for(int ir = 0; ir < Tr; ir++)
      {
        for(int ic = 0; ic < Tc; ic++)
        {
          for(int um = 0; um < Um; um++)
          {
#pragma HLS UNROLL
            data_out_t data = output_fifo_fullp[um].read();
            output_fifo[um] << data_in_t(data >> (data_in_t::width - 1));

            outcs_hw[um] += data;
          } // um
        } // ic
      } // ir
    } // ito


    data_out_t outcs_sum = 0;
    for(int um = 0; um < Um; um++)
    {
#pragma HLS UNROLL
      outcs_sum += outcs_hw[um];
      outcs_hw[um] = 0;
    }
    *outcs = data_in_t(outcs_sum >> (data_in_t::width - 1));
  }
} // hw_outcs()

void hw_compare_cs(
  bool end,
  data_in_t incs,
  data_in_t outcs,
  ap_uint<ceillog2(TILES)> tile,
  ap_uint<FAILED_BITS> failed[FAILED_SIZE]
)
{
  static ap_uint<FAILED_BITS> failed_hw;
#pragma HLS RESOURCE variable=failed_hw core=RAM_S2P_LUTRAM

  if(end) {
    failed_hw[tile % FAILED_BITS] = (incs != outcs) ? ap_uint<1>(1) : ap_uint<1>(0);

    if(((tile % FAILED_BITS) == (FAILED_BITS - 1)) || (tile == TILES - 1))
      failed[tile / FAILED_BITS] = failed_hw;
  }
} // hw_compare_cs()

#endif
