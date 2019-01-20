cmake_minimum_required (VERSION 3.0)
project (satnow)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++14 -Wall -pedantic")

include(ExternalProject)
ExternalProject_Add(sgp4_download
  GIT_REPOSITORY https://github.com/dnwrnr/sgp4
  GIT_TAG 6b47861cd47a6e31841260c47a52b579f8cf2fa9
  PREFIX ${CMAKE_BINARY_DIR}/third-party/sgp4
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/third-party/sgp4
  BUILD_COMMAND ${CMAKE_COMMAND} --build . --target sgp4
  INSTALL_COMMAND ""
)
include_directories(${CMAKE_SOURCE_DIR}/third-party/sgp4/libsgp4)
link_directories(${CMAKE_BINARY_DIR}/third-party/sgp4/src/sgp4_download-build/libsgp4)

add_executable (satnow main.cc db.cc display.cc)

target_link_libraries(satnow sgp4 curl sqlite3)
add_dependencies(satnow sgp4_download)

find_library(HAVE_CURSES ncurses)
find_library(HAVE_MENU menu)
if (HAVE_CURSES AND HAVE_MENU)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_GUI")
  target_link_libraries(satnow ncurses menu)
endif()
