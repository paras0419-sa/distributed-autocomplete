add_library(autocomplete_query
  src/Tokenizer.cpp
  src/QueryExecutor.cpp
)
target_include_directories(autocomplete_query PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(autocomplete_query PUBLIC
  autocomplete::common
  autocomplete::memory
  autocomplete::index)
autocomplete_set_warnings(autocomplete_query)
autocomplete_apply_sanitizers(autocomplete_query)
add_library(autocomplete::query ALIAS autocomplete_query)
