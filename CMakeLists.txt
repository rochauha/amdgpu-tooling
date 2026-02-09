cmake_minimum_required(VERSION 3.20)

set(ROCM_PATH
    ""
    CACHE PATH "Path to ROCm install directory")

if("${ROCM_PATH}" STREQUAL "")
  message(
    FATAL_ERROR
      "\n ROCM_PATH not set"
      "Please provide it using: cmake -DROCM_PATH=/path/to/rocm/install \n")
endif()

set(CMAKE_C_COMPILER "${ROCM_PATH}/bin/amdclang")
set(CMAKE_CXX_COMPILER "${ROCM_PATH}/bin/amdclang++")

project(amdgpu-tooling LANGUAGES CXX)
# ALL REGULAR TOOLS

add_executable(expand-args expand-args.cpp)
target_include_directories(
  expand-args PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third-party/msgpack/include)

add_executable(update-note-phdr update-note-phdr.cpp)
target_include_directories(
  update-note-phdr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third-party/elfio-3.11)

add_executable(extract-fatbin extract-fatbin.cpp)
target_include_directories(
  extract-fatbin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third-party/elfio-3.11)

add_executable(extract-gpubin extract-gpubin.cpp)

add_executable(update-fatbin update-fatbin.cpp)

add_executable(update-exec update-exec.cpp)
target_include_directories(
  update-exec PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third-party/elfio-3.11)

# SPECIAL CASE FOR PRELOAD

# Paths for hipcc and the preload file
set(HIPCC "${ROCM_PATH}/bin/hipcc")
set(PRELOAD_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/preload.cpp")
set(PRELOAD_SO "${CMAKE_CURRENT_BINARY_DIR}/preload.so")

# Actual command to build preload.so
add_custom_command(
  OUTPUT "${PRELOAD_SO}"
  COMMAND ${HIPCC} -D__HIP_PLATFORM_AMD__ -x c++ -shared -fpic
          -I/opt/rocm-6.0.0/include/ "${PRELOAD_SOURCE}" -o "${PRELOAD_SO}"
  DEPENDS "${PRELOAD_SOURCE}"
  COMMENT "Building ${PRELOAD_SOURCE} with ${HIPCC}"
  VERBATIM)

add_custom_target(PreloadFile ALL DEPENDS "${PRELOAD_SO}")
