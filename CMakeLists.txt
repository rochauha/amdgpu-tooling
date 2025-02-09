cmake_minimum_required(VERSION 3.20)

project(amdgpu-tooling LANGUAGES CXX)

set(CMAKE_MODULE_PATH
  "${Dyninst_DIR}"
  "${Dyninst_DIR}/Modules"
  ${CMAKE_MODULE_PATH}
)

# Import the Dyninst components
find_package(Dyninst REQUIRED COMPONENTS symtabAPI dynElf)
# Read the cache generated from building Dyninst
load_cache(${Dyninst_DIR}
  # READ_WITH_PREFIX DYNINST_
  Boost_LIBRARIES
  Boost_INCLUDE_DIRS
  Boost_LIBRARY_DIRS
  Boost_DEFINES
  TBB_INCLUDE_DIRS
)

add_library(amdgpu-tooling
  KernelDescriptor.cpp
  KdUtils.cpp
  DyninstUtility.cpp
)

target_include_directories(amdgpu-tooling PRIVATE
  ${DYNINST_INCLUDE_DIR}
  ${DYNINST_Boost_INCLUDE_DIRS}
  ${DYNINST_TBB_INCLUDE_DIRS}

  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/msgpack/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/elfio-3.11
)

target_link_libraries(amdgpu-tooling PRIVATE
  Dyninst::symtabAPI
)

target_compile_definitions(amdgpu-tooling PRIVATE ${DYNINST_Boost_DEFINITIONS})
target_link_directories(amdgpu-tooling PRIVATE ${DYNINST_Boost_LIBRARY_DIRS})


add_executable(kernel-info
               kernel-info.cpp)

target_include_directories(kernel-info PRIVATE
  ${DYNINST_INCLUDE_DIR}
  ${DYNINST_Boost_INCLUDE_DIRS}
  ${DYNINST_TBB_INCLUDE_DIRS}
)

target_compile_definitions(kernel-info PRIVATE ${DYNINST_Boost_DEFINITIONS})
target_link_directories(kernel-info PRIVATE ${DYNINST_Boost_LIBRARY_DIRS})

target_link_libraries(kernel-info PRIVATE
  amdgpu-tooling
  Dyninst::symtabAPI
  ${DYNINST_Boost_LIBRARIES}
)

add_executable(update-kd
               update-kd.cpp)

target_include_directories(update-kd PRIVATE
  ${DYNINST_INCLUDE_DIR}
  ${DYNINST_Boost_INCLUDE_DIRS}
  ${DYNINST_TBB_INCLUDE_DIRS}
)

target_compile_definitions(update-kd PRIVATE ${DYNINST_Boost_DEFINITIONS})
target_link_directories(update-kd PRIVATE ${DYNINST_Boost_LIBRARY_DIRS})

target_link_libraries(update-kd PRIVATE
  amdgpu-tooling
  Dyninst::symtabAPI
  ${DYNINST_Boost_LIBRARIES}
)

# expand-args requires libmsgpack-dev
add_executable(expand-args
               expand-args.cpp)


add_executable(extract-fatbin extract-fatbin.cpp)

target_include_directories(extract-fatbin PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/elfio-3.11
)

add_executable(extract-gpubin extract-gpubin.cpp)

add_executable(update-fatbin update-fatbin.cpp)

# rep-tool
add_executable(rep-tool
               rep-tool.cpp
               DyninstUtility.cpp
               )

target_include_directories(rep-tool PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/elfio-3.11
)
