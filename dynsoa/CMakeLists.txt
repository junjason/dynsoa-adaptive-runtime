cmake_minimum_required(VERSION 3.16)
project(DynSoA LANGUAGES CXX)

option(DYNSOA_BUILD_TESTS "Build DynSoA tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(dynsoa SHARED
  src/dynsoa.cpp
  src/schema.cpp
  src/entity_store.cpp
  src/layout.cpp
  src/metrics.cpp
  src/scheduler.cpp
  src/kernels.cpp
)

target_include_directories(dynsoa
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(dynsoa PRIVATE DYNSOA_BUILD_DLL)

install(TARGETS dynsoa
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
install(DIRECTORY include/ DESTINATION include)

if(DYNSOA_BUILD_TESTS)
  add_executable(dynsoa_smoke tests/smoke_main.cpp)
  target_link_libraries(dynsoa_smoke PRIVATE dynsoa)

  add_executable(dynsoa_boids tests/boids_main.cpp)
  target_link_libraries(dynsoa_boids PRIVATE dynsoa)

  add_executable(dynsoa_boids_multibackend tests/boids_multibackend.cpp)
  target_link_libraries(dynsoa_boids_multibackend PRIVATE dynsoa)
endif()
