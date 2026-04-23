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
