add_executable(serving_node src/main.cpp)
target_link_libraries(serving_node PRIVATE
  autocomplete::common
  autocomplete::memory
  autocomplete::index
  autocomplete::query)
autocomplete_set_warnings(serving_node)
autocomplete_apply_sanitizers(serving_node)
