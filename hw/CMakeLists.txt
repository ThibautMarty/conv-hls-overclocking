set(CMAKE_CXX_COMPILER sds++)

set(SDS_FLAGS
  "${SDSARGS} -sds-pf ${PLATFORM} \
  -sds-hw hw_toplevel hw/conv_accel.cpp -clkid ${CLKID} -sds-end \
  -maxthreads 2 -maxjobs 2"
)

set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} ${SDS_FLAGS}"
)

set(CMAKE_LD_FLAGS
  "${SDS_FLAGS} -dmclkid ${DMCLKID}"
)

# sds compiler needs the file convention %.o <=> %.cpp
# this enables that instead of %.cpp.o
# https://cmake.org/pipermail/cmake/2012-August/051638.html
set(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE 1)

# Tell cmake to remove _sds directory on clean
set_directory_properties(PROPERTIES
  ADDITIONAL_MAKE_CLEAN_FILES
  "_sds"
)
# Accelerator is compiled as a shared library
add_library(convolution SHARED
  convolution.cpp
  conv_accel.cpp
)

set_target_properties(convolution PROPERTIES
  PUBLIC_HEADER ../inc/convolution.h
)

# target_include_directories(
#   convolution PRIVATE
#   "${XILINXPATH}/SDx/${SDXVERSION}/target/aarch32-linux/include/"
# )

target_link_libraries(convolution
  sds_lib
  ${CMAKE_THREAD_LIBS_INIT}
)
