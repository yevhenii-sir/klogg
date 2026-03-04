find_package(QT NAMES Qt6 Qt5 COMPONENTS Test REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test REQUIRED)

function(klogg_configure_test_target target)
  if(MSVC)
    target_link_options(
      ${target}
      PRIVATE
        "$<$<CONFIG:RelWithDebInfo>:/DEBUG:NONE>"
        "$<$<CONFIG:RelWithDebInfo>:/INCREMENTAL:NO>"
    )
  endif()
endfunction()

add_subdirectory(helpers)
add_subdirectory(unit)
add_subdirectory(ui)
if(KLOGG_USE_VECTORSCAN)
  add_subdirectory(vectorscan)
endif()

add_dependencies(klogg_itests file_write_helper)
add_dependencies(ci_build klogg_tests klogg_itests)






