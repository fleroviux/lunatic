include(CheckSymbolExists)

# Universal Binary support for MacOS
if (CMAKE_OSX_ARCHITECTURES)
  set(ARCHITECTURE "${CMAKE_OSX_ARCHITECTURES}")
  return()
endif()

# Test for architecture and add it to the ARCHITECTURE list 
function(test_architecture arch_name arch)
  if (NOT DEFINED ARCHITECTURE)
    set(CMAKE_REQUIRED_QUIET YES)
    check_symbol_exists("${arch_name}" "" DETECT_ARCHITECTURE_${arch})
    unset(CMAKE_REQUIRED_QUIET)

    if (DETECT_ARCHITECTURE_${arch})
      set(ARCHITECTURE "${arch}" PARENT_SCOPE)
    endif()

    unset(DETECT_ARCHITECTURE_${arch} CACHE)
  endif()
endfunction()

test_architecture("__aarch64__" arm64)
test_architecture("__ARM64__" arm64)
test_architecture("_M_ARM64" arm64)

test_architecture("__amd64" x86_64)
test_architecture("__x86_64__" x86_64)
test_architecture("__x86_64" x86_64)
test_architecture("_M_X64" x86_64)

test_architecture("__riscv" riscv)