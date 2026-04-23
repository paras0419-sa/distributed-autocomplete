find_package(GTest REQUIRED)

add_executable(serving_node_e2e_test serving_node_e2e_test.cpp)
target_link_libraries(serving_node_e2e_test PRIVATE
  GTest::gtest_main
  autocomplete::index
  autocomplete::memory
  autocomplete::query)
target_compile_definitions(serving_node_e2e_test PRIVATE
  FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
autocomplete_set_warnings(serving_node_e2e_test)
autocomplete_apply_sanitizers(serving_node_e2e_test)
add_test(NAME serving_node_e2e_test COMMAND serving_node_e2e_test)
