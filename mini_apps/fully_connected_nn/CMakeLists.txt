add_test_app(fully_connected_nn
  axpby.cpp
  cross_entropy_loss.cpp
  dense_layer.cpp
#  matmul.cpp
  relu.cpp
# softmax.cpp
  sum_along_columns.cpp
  ../../main.sample.cpp
  )

target_compile_options(fully_connected_nn PRIVATE -fenable-ripple)
target_compile_options(fully_connected_nn PRIVATE -Wno-vla-cxx-extension)
target_include_directories(fully_connected_nn PRIVATE ${RIPPLE_TEST_SUITE_INCLUDE_DIR})
