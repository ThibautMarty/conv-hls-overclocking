# HLS Convolution with overclocking

*For better formatting, table of contents, etc, please visit [this page](https://thibautmarty.fr/posts/fpt-paper-resources).*

This file gives overview, instructions and technical details for the project used in our paper "Enabling Overclocking through Algorithm-Level Error Detection" published in [FPT 2018](http://fpt18.sakura.ne.jp/).

* [FPT'18 paper](https://hal.inria.fr/hal-01942429/)
* [Github link](https://github.com/ThibautMarty/conv-hls-overclocking)


# Overview

The code includes an accelerator for convolution computation found in [CNN](https://en.wikipedia.org/wiki/Convolutional_neural_network).
The accelerator is entirely designed using SDSoC and Vivado HLS.

Its working frequency can be modified at runtime, i.e you can overclock it.
In addition to the main convolution, lightweight extra computations allow the detection of timing errors.

For more information, context and results, read the paper ;)

The computation is done in fixed-point. The wordlength can be changed before synthesis.
All convolution parameters (except stride) can be changed as well.

## Hardware architecture

Important notice: this work is not meant to be the fastest FPGA convolution ever.
It supports our research work.

Computation is built around a tile-based dataflow:

1. transfer of operands
2. convolution computation
3. transfer of results

Those steps are overlapped when computing several tiles.

The input-checksum is computed in parallel of the main convolution kernel.
Simple sums are also performed during operands transfer.
The output-checksum is interposed between the convolution and the output transfer.
The overall latency is impacted by the checksums computation only by a few cycles.

```
        tile input          tile weight
            | |                  | |
            | |                  | |
            \ /                  \ /
    +--------v--------+  +--------v--------+
    |                 |  |                 |
    |                 |  |                 |
    | input  transfer |  | weight transfer |
    |                 |  |                 |
    |   (incs  sums   |  |   (incs sums    |
    |  over  inputs)  |  |  over weights)  |
    |                 |  |                 |
    +---+ +-------+---+  +---+ +---------+-+
        | |       |          | |         |
        | |       +----------| |-----+   |
        | |                  | |     |   |
        | |     +------------+ |     |   |
        | |     | +------------+     |   |
        \ /     \ /                  |   |
+--------v-------v--------+     +----v---v----+
|                         |     |             |
|                         |     |             |
|                         |     |    input    |
|       convolution       |     |             |
|                         |     |  checksum   |
|                         |     |             |
|                         |     |             |
+-----------+ +-----------+     +--------+----+
            | |                          |
            | |                          |
            \ /                          |
      +------v------+                    |
      |             |                    |
      |             |                    |
      |   output    |                    |
      |             |                    |
      |  checksum   |                    |
      |             |                    |
      |             |                    |
      +-----+ +--+--+                    |
            | |  |                       |
            | |  +-------------------+   |
            \ /                      |   |
      +------v------+           +----v---v----+
      |             |           |             |
      |             |           |             |
      |   output    |           |  checksums  |
      |             |           |             |
      |  transfer   |           |   compare   |
      |             |           |             |
      |             |           |             |
      +-----+ +-----+           +------+------+
            | |                        |
            | |                        |
            \ /                        |
             v                         v
        tile output                tile error
```

The main convolution kernel is classic, although few modifications has been done to simplify transfers and checksums computation.

Everything is packaged in only one IP with Vivado HLS.
Another solution would have been to make three IPs for standard convolution, input-checksum and output-checksum, connected with e.g AXI stream interfaces.
The only constraint is to use asynchronous FIFOs for I/O to allow overclocking.

## Software interface

The hardware is synthesized with Vivado, and SDSoC build the wrapper in a shared library.
This allows to use the same hardware (through the same shared library and bitfile) with several executables. The synthesis has to be done only once.

The main library entry point is (optional arguments removed):
```c++
int convolution(
  data_in_t input[BATCHES][N][RR][CC],
  data_in_t weights[BATCHES][N][M][K][K],
  data_in_t output[BATCHES][M][R][C],
  bool failed[TILES]
);
```
This function manages memories and call the accelerator to perform the convolution computation.
All dimension sizes can be set in build configuration (see [`ccmake` step](#build-accelerator-and-executables) below).

# Build and run the project

Building the project requires [SDx 2018.2](https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/sdx-development-environments.html).

Note: everything can be built using command line tools only, as this is my main workflow.
Using GUIs is still possible, and needed to e.g see Vivado HLS scheduling.
This will be explained how on relevant steps.

Start by cloning the project with `git`:
```bash
git clone --recursive https://github.com/ThibautMarty/conv-hls-overclocking
cd conv-hls-overclocking
```

## Build overclocking-enabled platform

These steps build a SDSoC platform targetting zc706 board, with a few additions enabling overclocking.

The software part of the platform is the same as the default one. Copy it with:
```bash
pushd pfm
cp -r /opt/Xilinx/SDx/2018.2/platforms/zc706/sw/ sw
```

To build a platform with 100 MHz target frequency[^1], only from command line[^2], do:
```bash
freq=100
mkdir zc706oc-$freq
pushd zc706oc-$freq
vivado -mode batch -source ../pfm_vivado.tcl -tclargs $freq
xsct -sdx ../pfm_sdx.tcl
popd
popd
```

[^1]: currently, it only works with this frequency
[^2]: but for whatever reason, the `xsct` command needs a running X server of Xvfb to succeed

Of course, the Vivado project and the SDSoC platform can be modified.
The Vivado `tcl` file can be sourced from the GUI, or the `.xpr` file can be opened with Vivado after afterwards.

The path to be used as platform in `cmake` options can be found with:
```bash
realpath ./pfm/zc706oc-$freq/output/*/export/*
```

To get more information on the platform, run:
```bash
sdscc -sds-pf-info ./pfm/output/*/export/*
```
This gives, among other things, available clocks with ID and frequency.
Clock #1 is the overclocked one (to be used for accelerator), clock #0 is the default one which is not overclocked (to be used for data movers).

Clock #1 displayed frequency is the default one (before changes at runtime).
This frequency is used as **target frequency** by Vivado HLS and Vivado.
See below on how to [modify this frequency](#modify-target-frequency).

## Build accelerator and executables

The project uses `cmake`. It allows to make "out of source" builds (in a different directory than the source one), and to have any number of such directories.
Each build directory can have different build options.

This will build the project in the `build` directory using default options:
```bash
mkdir build
pushd build
cmake ..
make
popd
```
You may need to configure the project (after the `cmake` step) in order to configure paths and platform, enabling tests, options such as tile size, etc, with:
```bash
ccmake .
```
Each configuration option is documented.
Run `make` again will build again what is needed with the new options.

### Log and report files

From the build directory, in the `hw/_sds` subdirectory, you will find these log and report files:

* `reports/sds_conv_accel.rpt`: HLS report (performance estimates, utilization estimates)
* `reports/sds_conv_accel.log`: `sds++` and HLS log (warnings, pipelining, unrolling…)
* `reports/sds.log`: `sds++` log
* `reports/sds.rpt`: main report file, with timing and utilization report
* `reports/data_motion.html`: data motion report
* `vhls/hw_toplevel/solution/syn/report/*.rpt`: all HLS reports (e.g for dataflow actors)


### Use Vivado HLS GUI

If needed, you can open the Vivado HLS project with the GUI.
This need the Vivado HLS project to be created by SDSoC.
Thus you need to go through the command line steps until the `make` step.
You can kill the process when `Moving function hw_toplevel to Programmable Logic` is displayed (invocation of Vivado HLS).

To make SDSoC build the Vivado HLS project without going through full synthesis, run instead of `make`:
```bash
make cmake_check_build_system # needed if project was never build OR if any cmake options were changed
make -f hw/Makefile conv_accel.o
```

Then, run:
```bash
cd hw/_sds/vhls
vivado_hls -p hw_toplevel
```
All directives from SDSoC should be set and the correct includes file (generated by cmake) should be reachable and used by Vivado HLS.

## Run on zc706 board

Start by copying the files to an SD card: `sd_card` folder (including shared library), bitfile, and executables (named `*.bin`):
```bash
rsync -avz ./build/{hw/sd_card/,hw/libconvolution.so.bit,*.bin} /path/to/sdcard
```

Boot the board on the SD card, and connect for instance using a USB serial link:
```bash
screen /dev/ttyUSB0 115200
```
then, execute on the board:
```bash
Zynq> fatload mmc 0 0x1000000 image.ub
Zynq> bootm 0x1000000

root@zc706:~# cd /mnt
root@zc706:/mnt# export LD_LIBRARY_PATH=.
root@zc706:/mnt# ./error_rate.bin 1000 200 | tee log
```

# Technical details

## Files overview

The file hierarchy follows my [SDSoC cmake template](https://gitlab.inria.fr/tmarty/sdsoc-cmake-template).
In summary:

* `hw` contains HLS code and call wrapper, compiled with `sds++` (i.e SDSoC) as a shared library
* `inc` contains headers
* `tests` contains test code using [Google Test framework](https://github.com/google/googletest)
* `src` contains non-HLS code, including executables' `main`, compiled with the cross-compiler, dynamically linked to the shared library
* `options.cmake` contains cmake options, used in `inc/options.h.in` and `inc/options.sw.h.in` headers. Those files are processed by cmake, you can find resulting files in the `inc` subdirectory of build directories

## Hardware functions hierarchy

### hw_toplevel()

Top-level function for Vivado HLS.
It loops over tiles and call all others function for each tile, building the dataflow.
There is two levels of tile loops:

* output tile (grouping three dimensions)
* input tile

One output tile is computed from several input tiles. The signals `start` and `end` tell each dataflow actor if the current input tile is the first or the last one used to compute the current output tile.


### hw_recv_input()

Copy input tile from external memory to BRAM.
Perform an simple early computation step for input-checksum corresponding to (in the paper): $X_{n,i,j}$

### hw_recv_weights()

Copy weight tile from external memory to BRAM.
Perform an simple early computation step for input-checksum corresponding to (in the paper): $\sum_{m = 0}^{M - 1} w_{m,n,i,j}$

### hw_incs()

Compute input-checksum.

### hw_conv()

Compute convolution.

### hw_outcs()

Compute output checksum and shrink output to the input format.

### hw_send_output()

Copy output from BRAM to external memory.

### hw_compare_cs()

Compare the checksums and send error bits. The granularity is one output tile.

## Hacking

### Modify target frequency

To choose another target frequency for synthesis, you need to modify the platform itself, and build it again.

Two `cmake` options need to be modified:

* `INPUT_CLK`: clocking wizard's input frequency (needed to compute correct parameters)
* `SAFE_CLK`: synthesis target frequency (to be used in source code for display, speedup, etc)

### Overclocking interface

The project use the *mixed-mode clock manager (MMCM)* module to change clock at runtime.
It uses the *clocking wizard* Xilinx IP with dynamic reconfiguration enabled on AXI bus.
The files `inc/clkwiz.h` and `src/clkwiz.cpp` gives a simple programming interface to control the IP.

The `Clkwiz` object contructor takes 3 arguments: minimum frequency, maximum frequency and step between two adjacents frequency (all in MHz).
It will try to find a list of parameters that fit requirements.
Example invocation:
```c++
Clkwiz clkwiz = new Clkwiz(INPUT_CLK, INPUT_CLK * 2.f, .1f);
```
Granularity is of course not fixed, as this is based on frequency multipliers and dividers. 0.1 MHz step can be mostly honored.

The functions `restart`, `next`, `previous` respectively set the frequency at the minimum, the next and the previous one in the list.
They return the target frequency (computed from multipliers and dividers parameters) as `float`.

The `end` function allows to simply scan all frequencies:
```c++
for(clkwiz->restart(); !clkwiz->end(); freq = clkwiz->next())
```

Note that in order to work, it just needs a clocking wizard to be reachable through AXI bus.
The AXI base address can be modified if needed in the file `inc/clkwiz.h`:
```c++
#define BASEADDR  0x43C00000
```

### Add a new executable

To add a executable using the shared library and accelerator, follow this steps:

Write your program in c++ files in the `src` directory, including relevant headers:
```c++
#include "convolution.h" # accelerator wrapper interface
#include "clkwiz.h"      # overclocking management
```

Then edit the root `CMakeLists.txt` file to add the executable at the end.
Copy and paste other executable definitions and change name and sources files.
You should end up with three definitions: `add_executable`, `target_include_directories`, and `target_link_libraries`.
You're done! You can run `make` again.

### Use accelerator without overclocking

Using the project without overclocking is possible, e.g to use another board.
Changes these `cmake` options:

* `PLATFORM`: platform name (e.g `zybo`)
* `CLKID`: clock used for synthesis
* `DMCLKID`: clock used for data motion

You can retrieve clock IDs with e.g:
```bash
sdscc -sds-pf-info zybo
```

Optionally remove the line `add_definitions(-DENABLE_CLKWIZ)` from the root `CMakeLists.txt` file. This will remove all effective runtime clock configuration.

# Areas for improvement

Due to SDSoC philosophy ("hardware call"), the accelerator is designed to compute a fixed limited amount of tiles. Although this was sufficient for this prototype, it could be rewritten to work on a continuous stream of tiles.
It could be integrated with FPGA DNN frameworks such as [ChaiDNN](https://github.com/Xilinx/CHaiDNN).

Write a Python wrapper and integrate in a high-level deep learning library like [Keras](https://keras.io/) would make it much easier to get application-level errors, application-level speedup, etc.

# License

Distributed under the [GPLv3 license](https://www.gnu.org/licenses/gpl-3.0.en.html).
