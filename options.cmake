# These options can be set with command line: e.g cmake . -DTESTS=ON
# or by UI (e.g ccmake)

# Check tests before actual build
option(TESTS "Set to ON to build and run tests" OFF)

# Do ABFT on hw or sw
option(ENABLE_HARDWARE_ABFT "Set to ON to build accelerator with ABFT" ON)

option(NODSP "Do not use DSP for convolution kernel" OFF)

# Clocking wizard input clock (AXI clock)
set(INPUT_CLK 100.f CACHE STRING "Clocking wizard iput clock (MHz)")
set(SAFE_CLK 100.f CACHE STRING "Static safe frequency (MHz) - only used for speedup measurements")

# Datamover and accelerator clock IDs
set(DMCLKID 0 CACHE STRING "Data mover clock ID")
set(CLKID 1 CACHE STRING "Accelerator clock ID")

# Version of SDx for headers/libraries
set(SDXVERSION "2018.2" CACHE STRING "SDx version for headers/libraries (you still need to source settings64.sh)")
set(XILINXPATH "/opt/Xilinx" CACHE STRING "Path to Xilinx's tool installation directory")

# Target SDSoC platform, either name for a standard one, or an path
# (in which case it will be resolved to an absolute path)
set(PLATFORM "/path/to/platform/zc706oc" CACHE STRING "SDx platform (path or standard name)")
set(SDSARGS "--remote_ip_cache ~/ip_cache -sds-sys-config linux -sds-proc linux" CACHE STRING "sds compiler-specific arguments")


# number of batches (independent inputs)
set(BATCHES 2 CACHE STRING "batches")

# Tile sizes
set(Tr 13 CACHE STRING "Tile size (rows)")
set(Tc 13 CACHE STRING "Tile size (columns)")
set(Tn 32 CACHE STRING "Tile size (input feature maps) /!\ Needs to be a factor of Un")
set(Tm 64 CACHE STRING "Tile size (output feature maps) /!\ Needs to be a factor of Um")

# Unroll factor
# Note: HW uses 4 * Um * Un DSP for convolution, zybo has 80
set(Um 16 CACHE STRING "Unroll factor on M dimension")
set(Un 32 CACHE STRING "Unroll factor on N dimension")

# Convolution parameters
set(N 192 CACHE STRING "Convolution input feature maps")
set(M 128 CACHE STRING "Convolution output feature maps")
set(R 13 CACHE STRING "Convolution output rows")
set(C 13 CACHE STRING "Convolution output columns")
set(K 3 CACHE STRING "Convolution kernel size")
set(S 1 CACHE STRING "Convolution stride. /!\ Keep S=1. Non-strided convolution and checksums are not fully implemented!")
set(DATA_WL 16 CACHE STRING "Convolution data word length for i/o (doubled for internal results)")

# Enable 8 bits mode
if(DATA_WL LESS "2")
  set(NODSP ON)
endif()
# elseif(DATA_WL LESS "9")
# # elseif(DATA_WL EQUAL "8")
#   set(OPTIDSP ON)
#   math(EXPR OPTIDSPSTEPS "1 + (24 - ${DATA_WL}) / (2 * ${DATA_WL})")
# endif()
set(OPTIDSP OFF)

# Compute some values based on options
math(EXPR K2 "${K} * ${K}")
math(EXPR OUTPUT_FIFO_DEPTH "${Tr} * ${Tc} * ${Tm} / ${Um}")
