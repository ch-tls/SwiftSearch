# PlatformConfig.cmake — OS-specific conditional compilation
#
# Sets platform macros, link libraries, and compiler flags
# for Windows (MSVC) and Linux (GCC/Clang).

if(WIN32)
  set(CMAKE_WIN32_EXECUTABLE ON)
  add_compile_definitions(SWIFTSEARCH_PLATFORM_WINDOWS=1)
  set(PLATFORM_LIBS ws2_32)

  if(MSVC)
    add_compile_options(/MP /EHsc /utf-8 /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(NOMINMAX)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)

    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL /O2 /Oi /Gy")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF")
  elseif(MINGW)
    add_compile_definitions(NOMINMAX)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)

    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto -fvisibility=hidden")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto -static-libgcc -static-libstdc++")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -flto")
  endif()
elseif(UNIX AND NOT APPLE)
  add_compile_definitions(SWIFTSEARCH_PLATFORM_LINUX=1)

  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto -fvisibility=hidden")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -flto")

  find_package(Qt6DBus QUIET)
  if(Qt6DBus_FOUND)
    list(APPEND PLATFORM_LIBS Qt6::DBus)
  endif()
  find_package(Threads REQUIRED)
  list(APPEND PLATFORM_LIBS Threads::Threads)
endif()

message(STATUS "Platform libraries: ${PLATFORM_LIBS}")
