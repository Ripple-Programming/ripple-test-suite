add_test_app(
  x86_perf_tests
  rgba_to_rgb.cpp
  options_pricing.cpp
  ao_ripple.cpp
  ao_ref.cpp
  mandelbrot.cpp
  stencil.cpp
  ../../../main.sample.cpp
)

link_ispc_shared(x86_perf_tests options.ispc)
link_ispc_shared(x86_perf_tests ao.ispc)
link_ispc_shared(x86_perf_tests stencil.ispc)
link_ispc_shared(x86_perf_tests mandelbrot.ispc)

target_compile_options(x86_perf_tests PRIVATE -fenable-ripple)
target_include_directories(x86_perf_tests PRIVATE ${RIPPLE_TEST_SUITE_INCLUDE_DIR})
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(x86_perf_tests PRIVATE -march=native -O3)
endif()
