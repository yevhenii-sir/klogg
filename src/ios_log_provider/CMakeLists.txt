add_library(
  klogg_ios_log_provider STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include/ios_device_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/ios_syslog_receiver.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ios_device_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ios_syslog_receiver.cpp
)

set_target_properties(klogg_ios_log_provider PROPERTIES AUTOMOC ON)

target_include_directories(klogg_ios_log_provider PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_definitions(klogg_ios_log_provider PUBLIC KLOGG_WITH_IMOBILEDEVICE)

target_link_libraries(
  klogg_ios_log_provider
  PUBLIC Qt${QT_VERSION_MAJOR}::Core
         Qt${QT_VERSION_MAJOR}::Widgets
         unofficial::libimobiledevice::libimobiledevice
  PRIVATE klogg_logdata
          klogg_utils
)

if(KLOGG_USE_LTO)
  set_property(TARGET klogg_ios_log_provider PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()