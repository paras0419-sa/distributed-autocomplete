# cmake/Warnings.cmake
function(autocomplete_set_warnings target)
  target_compile_options(${target} PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion
    -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
    -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion
    -Werror
  )
endfunction()
