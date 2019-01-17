#include <iostream>
#include "convolution.h"
#include "golden_convolution.h"
#include "clkwiz.h"
#include "io.h"
#include <err.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
  data_in_t input[BATCHES][N][RR][CC];
  data_in_t weights[BATCHES][N][M][K][K];
  data_in_t output[BATCHES][M][R][C];
  bool abftfailed[TILES];
  int count;
  int imgs, img;
  float goal, freq;

  Clkwiz *clkwiz;

  // Runtime check compatibility with library
  if(!compatibility_check(M, N, R, C, K, S, BATCHES))
    errx(1, "executable incompatible with shared library\n");

  if(argc < 3)
    errx(0, "usage: %s nbimgs freq\n",
      argv[0]
    );
  int arg_channel = 1;
  imgs = atoi(argv[arg_channel++]);
  goal = atof(argv[arg_channel++]);

  print_convolution_constants();

  // We use clocking wizard to directly go to the goal frequency
  clkwiz = new Clkwiz(std::min(INPUT_CLK, goal) - 1, std::max(INPUT_CLK, goal) + 1, .01);
  clkwiz->restart();
  while((freq = clkwiz->next()) < goal);
  std::cerr << "frequency: " << freq << " (goal: " << goal << ')' << std::endl;

  for(img = 0; img < imgs; img++)
  {
    fill_random(input, weights);

    //           #image         number of image
    std::cout << img << '\t' << imgs << '\t';

    count = convolution(
      input,
      weights,
      output,
      abftfailed
    );

    //           abft detected error?    number of failed tiles  number of tiles
    std::cout << ((count > 0) ? 1 : 0) << '\t' << count << '\t' << (TILES) << std::endl;
  }

  delete clkwiz;

  return 0;
}
