#ifndef __GOLDEN_CONVOLUTION_H
#define __GOLDEN_CONVOLUTION_H

#include "convolution.h"

void golden_convolution(
  data_in_t input[BATCHES][N][RR][CC],
  data_in_t weights[BATCHES][N][M][K][K],
  data_in_t output[BATCHES][M][R][C]
);

#endif // __GOLDEN_CONVOLUTION_H
