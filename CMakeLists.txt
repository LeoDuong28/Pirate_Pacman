cmake_minimum_required(VERSION 3.16)
project(PiratePacman CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release flags for optimization
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# SDL2
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_mixer REQUIRED)

# Fallback for Homebrew SDL2 packages that don't ship CMake configs
if(NOT TARGET SDL2::SDL2)
  find_library(SDL2_LIB SDL2)
  add_library(SDL2::SDL2 UNKNOWN IMPORTED)
  set_target_properties(SDL2::SDL2 PROPERTIES IMPORTED_LOCATION "${SDL2_LIB}")
endif()

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
  src
  /opt/homebrew/include
  /opt/homebrew/include/SDL2
)

target_link_directories(${PROJECT_NAME} PRIVATE /opt/homebrew/lib)

target_link_libraries(${PROJECT_NAME} PRIVATE
  SDL2
  SDL2_image
  SDL2_ttf
  SDL2_mixer
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -O2)
