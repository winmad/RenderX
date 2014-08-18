cmake_minimum_required(VERSION 2.8)
project(RenderX)

#CC=/Users/Winmad/llvm/build/Debug+Asserts/bin/clang2
#CXX=/Users/Winmad/llvm/build/Debug+Asserts/bin/clang2

find_package(OPENGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLFW REQUIRED)
find_package(GLUT REQUIRED)
find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_search_module(GLFW REQUIRED glfw3)

#set(CMAKE_C_COMPILER "/Users/Winmad/llvm/build/Debug+Asserts/bin/clang2")
set(CMAKE_CXX_COMPILER "/Users/Winmad/llvm/build/Debug+Asserts/bin/clang2++")
#set(CMAKE_CXX_COMPILER "/opt/local/bin/g++")

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp -std=c++11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp -stdlib=libc++ -g")

set(OPENMP_INCLUDE_DIRS "~/Intel-OpenMP/libomp_oss/exports/common/include")
set(OPENMP_LIBRARY_DIRS "~/Intel-OpenMP/libomp_oss/exports/mac_32e/lib.thin/")

include_directories(
  ${OPENGL_INCLUDE_DIRS}
  ${GLEW_INCLUDE_DIRS}
  ${GLFW_INCLUDE_DIRS}
  ${GLUT_INCLUDE_DIRS}
  ${OPENCV_INCLUDE_DIRS}
  ${OPENMP_INCLUDE_DIRS}
  include
  external/include
)

link_directories(
  ${GLFW_LIBRARY_DIRS}
  ${OPENGL_LIBRARY_DIRS}
  ${GLUT_LIBRARY_DIRS}
  ${GLEW_LIBRARY_DIRS}
  ${OPENCV_LIBRARY_DIRS}
  ${OPENMP_LIBRARY_DIRS}
#  external/lib
)

set(ALL_LIBS
  ${OPENGL_LIBRARIES}
  ${GLUT_LIBRARIES}
  ${GLEW_LIBRARIES}
  ${GLFW_LIBRARIES}
  ${OpenCV_LIBS}
)

AUX_SOURCE_DIRECTORY(src/ CPP_SOURCES)
add_executable(RenderX ${CPP_SOURCES})
target_link_libraries(RenderX ${ALL_LIBS})
