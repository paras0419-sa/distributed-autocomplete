find_package(GTest REQUIRED)

function(autocomplete_add_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE GTest::gtest_main)
  autocomplete_set_warnings(${name})
  autocomplete_apply_sanitizers(${name})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

autocomplete_add_test(common_error_test common/error_test.cpp)
target_link_libraries(common_error_test PRIVATE autocomplete::common)

autocomplete_add_test(common_result_test common/result_test.cpp)
target_link_libraries(common_result_test PRIVATE autocomplete::common)

autocomplete_add_test(memory_string_arena_test memory/string_arena_test.cpp)
target_link_libraries(memory_string_arena_test PRIVATE autocomplete::memory)

autocomplete_add_test(index_entry_table_test index/entry_table_test.cpp)
target_link_libraries(index_entry_table_test PRIVATE autocomplete::index)

autocomplete_add_test(index_simple_trie_test index/simple_trie_test.cpp)
target_link_libraries(index_simple_trie_test PRIVATE autocomplete::index)

autocomplete_add_test(query_tokenizer_test query/tokenizer_test.cpp)
target_link_libraries(query_tokenizer_test PRIVATE autocomplete::query)
