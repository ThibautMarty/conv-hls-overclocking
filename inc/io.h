#ifndef __IO_H
#define __IO_H

#include "convolution.h"

void fill_random(
  data_in_t input[BATCHES][N][RR][CC],
  data_in_t weights[BATCHES][N][M][K][K]
);

#endif // __IO_H
