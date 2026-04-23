# cmake/Sanitizers.cmake
option(AUTOCOMPLETE_SANITIZE "Enable ASan + UBSan" ON)

function(autocomplete_apply_sanitizers target)
  if(AUTOCOMPLETE_SANITIZE)
    target_compile_options(${target} PRIVATE
      -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE
      -fsanitize=address,undefined)
  endif()
endfunction()
