add_executable(
  klogg_vectorscan_tests
  vectorscan_tests.cpp
)

target_link_libraries(
  klogg_vectorscan_tests
  PRIVATE klogg_ui
          klogg_utils
          Catch2
          Qt${QT_VERSION_MAJOR}::Test
          test_utils
          klogg_mimalloc_wrapper
)

add_dependencies(ci_build klogg_vectorscan_tests)

add_test(
  NAME klogg_vectorscan_tests
  COMMAND klogg_vectorscan_tests -platform offscreen
)
