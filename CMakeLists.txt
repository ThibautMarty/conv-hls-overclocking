cmake_minimum_required(VERSION 3.3)

project(convolution CXX C)

# Use this to append search path for CMake's find* module (for e.g local installation of dependencies)
# list(APPEND CMAKE_PREFIX_PATH "~/bin/")

# Configuration options + file generation
include(options.cmake)
configure_file(inc/options.h.in inc/options.h)
configure_file(inc/options.sw.h.in inc/options.sw.h)
include_directories(${CMAKE_BINARY_DIR}/inc)

# Needed by sds compiler
find_package(Threads)

# Add link path for sds_lib
link_directories(
  "${XILINXPATH}/SDx/${SDXVERSION}/target/aarch32-linux/lib/"
)

# Ubiquitous compilation option
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS
  "-O3 -Wall \
  -Wno-unknown-warning-option \
  -Wno-unused-label \
  -Wno-unknown-pragmas \
  -Wno-uninitialized \
  -Wno-int-in-bool-context -Wno-mismatched-tags" # <= avoid warnings in Xilinx headers
)

# Remove Xilinx warning on hls_fpo.h
add_definitions(-DHLS_NO_XIL_FPO_LIB)

# Enable real clkwiz for executables, will be disabled in tests
add_definitions(-DENABLE_CLKWIZ)

# Headers path
include_directories(inc)

# Hardware code subdirectory
add_subdirectory(hw)

# Tests subdirectory
if(TESTS)
  add_subdirectory(tests)
endif()

# Host code subdirectory
# add_subdirectory(host)

# Use cross-compiler for everything (overwritten in other CMakeLists.txt)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)


# Executables (linked to convolution library)

# CNN error impact measurement executable
add_executable(error_rate.bin
  src/error_rate.cpp
  src/clkwiz.cpp
  src/golden_convolution.cpp
  src/io.cpp
)
target_include_directories(
  error_rate.bin PRIVATE
  "${XILINXPATH}/SDx/${SDXVERSION}/target/aarch32-linux/include/"
  "${XILINXPATH}/Vivado/${SDXVERSION}/include/"
)
target_link_libraries(error_rate.bin
  convolution
  sds_lib
  ${CMAKE_THREAD_LIBS_INIT}
)
