# CompilerWarnings.cmake — Google C++ Style warning set
#
# Applies strict warning flags aligned with the Google C++ Style Guide
# to the current target via swiftsearch_enforce_warnings().

option(SWIFTSEARCH_WERROR "Treat warnings as errors" OFF)

function(swiftsearch_enforce_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE
      /W4
      /permissive-
      /utf-8
      /Zc:__cplusplus
      /Zc:preprocessor
      /wd4244   # conversion, may lose data
      /wd4267   # conversion from size_t
      /wd4100   # unreferenced formal parameter (too noisy)
      /wd4127   # conditional expression is constant
    )
    if(SWIFTSEARCH_WERROR)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wnon-virtual-dtor
      -Wold-style-cast
      -Wcast-align
      -Wunused
      -Woverloaded-virtual
      -Wconversion
      -Wsign-conversion
      -Wnull-dereference
      -Wdouble-promotion
      -Wformat=2
      -Wimplicit-fallthrough
      -Wno-unused-parameter
    )

    if(SWIFTSEARCH_WERROR)
      target_compile_options(${target} PRIVATE -Werror)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
      target_compile_options(${target} PRIVATE
        -Wno-c++98-compat-pedantic
        -Wno-old-style-cast
        -Wno-switch-enum
      )
    endif()

    target_compile_options(${target} PRIVATE
      $<$<CONFIG:Debug>:-g -O0>
      $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
  endif()
endfunction()
