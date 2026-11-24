cmake_minimum_required(VERSION 3.20)
project(xer_unordered_flat_table_perf LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(DEFINED ENV{WSL_DISTRO_NAME} OR CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  set(_TP_DEFAULT "/mnt/d/ee2r44/Desktop/计算机学习资料/04_开源库与源码")
else()
  set(_TP_DEFAULT "d:/ee2r44/Desktop/计算机学习资料/04_开源库与源码")
endif()
set(THIRD_PARTY_ROOT "${_TP_DEFAULT}" CACHE PATH "Boost / Abseil / xxHash parent")
set(BOOST_ROOT "${THIRD_PARTY_ROOT}/Boost" CACHE PATH "")
set(XXHASH_ROOT "${THIRD_PARTY_ROOT}/xxHash" CACHE PATH "")
set(ABSL_ROOT "${THIRD_PARTY_ROOT}/Abseil" CACHE PATH "")
if(NOT EXISTS "${ABSL_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR "Abseil not found at ABSL_ROOT=${ABSL_ROOT}")
endif()

set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory("${ABSL_ROOT}" "${CMAKE_BINARY_DIR}/absl-build" EXCLUDE_FROM_ALL)

set(XER_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
set(XER_INCLUDE_DIR "${XER_ROOT}/include" CACHE PATH
  "xer public headers (use ab_mirror copy for A/B; never edit include/)")
add_executable(xer_unordered_flat_table_perf
  "${CMAKE_CURRENT_LIST_DIR}/../xer_unordered_flat_table_perf.cpp"
  "${XER_INCLUDE_DIR}/xer/env/xer_env.cpp"
)

target_compile_definitions(xer_unordered_flat_table_perf PRIVATE
  NDEBUG
  XXH_INLINE_ALL
)

target_include_directories(xer_unordered_flat_table_perf PRIVATE
  "${XER_INCLUDE_DIR}"
  "${BOOST_ROOT}"
  "${XXHASH_ROOT}"
)

target_link_libraries(xer_unordered_flat_table_perf PRIVATE
  absl::flat_hash_map
  absl::flat_hash_set
)

if(MSVC)
  target_compile_options(xer_unordered_flat_table_perf PRIVATE /O2 /EHsc /std:c++latest /W3)
else()
  target_compile_options(xer_unordered_flat_table_perf PRIVATE -O3 -DNDEBUG)
  find_package(Threads REQUIRED)
  target_link_libraries(xer_unordered_flat_table_perf PRIVATE Threads::Threads)
endif()

set(PERF_SUFFIX "unknown" CACHE STRING "Output name suffix: msvc|gcc|clang")
set_target_properties(xer_unordered_flat_table_perf PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.."
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_LIST_DIR}/.."
  OUTPUT_NAME "xer_unordered_flat_table_perf_${PERF_SUFFIX}"
)
