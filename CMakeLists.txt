cmake_minimum_required(VERSION 4.3)
project(nomoreprofile C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DOTNET_RUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/runtime")
set(CORECLR_INCLUDE_DIR "${DOTNET_RUNTIME_DIR}/src/coreclr/inc")
set(CORECLR_PAL_INCLUDE_DIR "${DOTNET_RUNTIME_DIR}/src/coreclr/pal/prebuilt/inc")

add_executable(nomoreprofile main.cc diagnostics_client.cc)

add_library(nomoreprofile_profiler SHARED
    hook_profiler.cc
    class_factory.cc
    dllmain.cc
        ilrewriter_compat.cc
    "${DOTNET_RUNTIME_DIR}/src/tests/profiler/native/profiler.cpp"
    "${DOTNET_RUNTIME_DIR}/src/tests/profiler/native/guids.cpp"
)

target_include_directories(nomoreprofile PRIVATE
    "${CORECLR_INCLUDE_DIR}"
    "${CORECLR_PAL_INCLUDE_DIR}"
)

target_include_directories(nomoreprofile_profiler PRIVATE
    "${CORECLR_INCLUDE_DIR}"
    "${CORECLR_PAL_INCLUDE_DIR}"
    "${DOTNET_RUNTIME_DIR}/src/tests/profiler/native"
    "${DOTNET_RUNTIME_DIR}/src/tests/profiler/native/rejitprofiler"
)

target_compile_definitions(nomoreprofile_profiler PRIVATE WIN32_LEAN_AND_MEAN SOS_INCLUDE)
target_link_libraries(nomoreprofile_profiler PRIVATE ole32)

find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format
             PATHS "C:/Program Files/LLVM/bin" NO_DEFAULT_PATH)
if(CLANG_FORMAT_EXECUTABLE)
  add_custom_target(format
    COMMAND "${CLANG_FORMAT_EXECUTABLE}" -i
      "${CMAKE_CURRENT_SOURCE_DIR}/main.cc"
      "${CMAKE_CURRENT_SOURCE_DIR}/diagnostics_client.cc"
      "${CMAKE_CURRENT_SOURCE_DIR}/diagnostics_client.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/hook_profiler.cc"
      "${CMAKE_CURRENT_SOURCE_DIR}/hook_profiler.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/class_factory.cc"
      "${CMAKE_CURRENT_SOURCE_DIR}/class_factory.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/dllmain.cc"
      "${CMAKE_CURRENT_SOURCE_DIR}/ilrewriter_compat.cc"
    COMMENT "Formatting native sources"
    VERBATIM)
endif()
