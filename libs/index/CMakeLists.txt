add_library(autocomplete_index
  src/SimpleTrie.cpp
)
target_include_directories(autocomplete_index PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(autocomplete_index PUBLIC
  autocomplete::common
  autocomplete::memory)
autocomplete_set_warnings(autocomplete_index)
autocomplete_apply_sanitizers(autocomplete_index)
add_library(autocomplete::index ALIAS autocomplete_index)
