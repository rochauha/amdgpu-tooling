cmake_minimum_required(VERSION 3.20)

project(amdgpu-tooling LANGUAGES CXX)

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
