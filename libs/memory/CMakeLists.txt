add_library(autocomplete_memory INTERFACE)
target_include_directories(autocomplete_memory INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(autocomplete_memory INTERFACE autocomplete::common)
add_library(autocomplete::memory ALIAS autocomplete_memory)
