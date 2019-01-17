find_package(GTest REQUIRED)

include_directories(${GTEST_INCLUDE_DIRS})

# Use host compiler
set(CMAKE_CXX_COMPILER g++)

remove_definitions(-DENABLE_CLKWIZ)

add_executable(runTests
  convolution.cpp
  ../hw/convolution.cpp
  ../hw/conv_accel.cpp
  ../src/golden_convolution.cpp
  ../src/io.cpp
  tools.cpp
  stubs.cpp
)

target_link_libraries(runTests
  ${GTEST_BOTH_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
)

# Include simulation headers (e.g ap_int.h)
target_include_directories(
  runTests PRIVATE
  "${XILINXPATH}/SDx/${SDXVERSION}/target/x86/include/"
  "${XILINXPATH}/Vivado/${SDXVERSION}/include/"
)

# target to run tests
add_custom_target(check
  COMMAND ./runTests
  DEPENDS runTests
)

# Build and run tests before building accelerator
add_dependencies(
  convolution
  check
)
