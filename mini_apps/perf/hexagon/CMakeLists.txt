add_test_app(
  hexagon_perf_tests
  ../../../main.sample.cpp
)

target_compile_options(hexagon_perf_tests PRIVATE -fenable-ripple)
target_include_directories(hexagon_perf_tests PRIVATE ${RIPPLE_TEST_SUITE_INCLUDE_DIR})
