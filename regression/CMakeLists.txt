add_test_app(regressions
  arithmetic.cpp
  reductions.cpp
  "../main.sample.cpp"
  )

target_compile_options(regressions PRIVATE -fenable-ripple)
target_include_directories(regressions PRIVATE ${RIPPLE_TEST_SUITE_INCLUDE_DIR})

if(RIPPLE_TEST_INCLUDE_HVX_RT_LIB)
  # add_subdirectory(math_functions)
  # target_link_libraries(regressions PRIVATE math_functions)
  add_subdirectory(scatter_gather)
  target_link_libraries(regressions PRIVATE scatter_gather)
  add_subdirectory(narrow_shift)
  target_link_libraries(regressions PRIVATE narrow_shift)
endif()
