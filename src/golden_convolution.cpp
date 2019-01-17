#include "golden_convolution.h"

// Software simple convolution reference
void golden_convolution(
  data_in_t input[BATCHES][N][RR][CC],
  data_in_t weights[BATCHES][N][M][K][K],
  data_in_t output[BATCHES][M][R][C]
)
{
  int b, row, col, to, ti, i, j;

  data_out_t output_tmp;

  for(b = 0; b < BATCHES; b++)
  {
    for(to = 0; to < M; to++)
    {
      for(row = 0; row < R; row++)
      {
        for(col = 0; col < C; col++)
        {
          output_tmp = 0;

          for(ti = 0; ti < N; ti++)
          {
            for(i = 0; i < K; i++)
            {
              for(j = 0; j < K; j++)
              {
                output_tmp +=
                  data_out_t(weights[b][ti][to][i][j]) *
                  data_out_t(input[b][ti][S * row + i][S * col + j]);
              } // j
            } // i
          } // ti

          output[b][to][row][col] = data_in_t(output_tmp >> (data_in_t::width - 1));
        } // col
      } // row
    } // to
  } // b
} // golden_convolution()
