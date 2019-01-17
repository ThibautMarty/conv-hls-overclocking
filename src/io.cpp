#include "io.h"
#include <stdlib.h>

void fill_random(
  data_in_t input[BATCHES][N][RR][CC],
  data_in_t weights[BATCHES][N][M][K][K]
)
{
  int b, to, ti, row, col, i, j;

  for(b = 0; b < BATCHES; b++)
  {
    for(ti = 0; ti < N; ti++)
    {
      for(to = 0; to < M; to++)
      {
        for(i = 0; i < K; i++)
        {
          for(j = 0; j < K; j++)
          {
            weights[b][ti][to][i][j] = rand();
          }
        }
      }
      for(row = 0; row < RR; row++)
      {
        for(col = 0; col < CC; col++)
        {
          input[b][ti][row][col] = rand();
        }
      }
    }
  }
}
