add_library(autocomplete_common INTERFACE)
target_include_directories(autocomplete_common INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(autocomplete::common ALIAS autocomplete_common)
