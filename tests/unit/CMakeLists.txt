# Add test cpp file
add_executable(klogg_tests
    cli_test.cpp
    linepositionarray_test.cpp
    patternmatcher_test.cpp
    tests_main.cpp
)

target_link_libraries(klogg_tests klogg_ui klogg_utils klogg_logging Catch2 Qt${QT_VERSION_MAJOR}::Test)
target_include_directories(klogg_tests PRIVATE "${CMAKE_SOURCE_DIR}/src/app")
set_target_properties(klogg_tests PROPERTIES AUTOMOC ON)

add_test(
    NAME klogg_tests
    COMMAND klogg_tests -platform offscreen
)
