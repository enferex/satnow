cmake_minimum_required (VERSION 3.0)
project (satnow)

SET(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} --std=c++14)

include(ExternalProject)
ExternalProject_Add(sgp4_download
  GIT_REPOSITORY https://github.com/dnwrnr/sgp4
  PREFIX ${CMAKE_BINARY_DIR}/third-party/sgp4
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/third-party/sgp4
  BUILD_COMMAND ${CMAKE_COMMAND} --build . --target sgp4
  INSTALL_COMMAND ""
)
include_directories(${CMAKE_SOURCE_DIR}/third-party/sgp4/libsgp4)
link_directories(${CMAKE_BINARY_DIR}/third-party/sgp4/src/sgp4_download-build/libsgp4)

add_executable (satnow main.cc)

target_link_libraries(satnow sgp4)
add_dependencies(satnow sgp4_download)
