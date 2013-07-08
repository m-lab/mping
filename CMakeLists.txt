cmake_minimum_required (VERSION 2.6)

project (mping)

set(PROJECT_ROOT_DIR ${PROJECT_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_ROOT_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_ROOT_DIR}/lib)

# set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_ROOT_DIR}/lib)
# set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

# Set up directories
set(MLAB_LIBRARIES_ROOT ${PROJECT_ROOT_DIR}/third_party/m-lab.libraries/trunk)
set(GTEST_ROOT ${MLAB_LIBRARIES_ROOT}/third_party/gtest-1.6.0)

set(CMAKE_CXX_FLAGS "-Wall -Werror -fPIC -fno-exceptions -fno-rtti")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -fno-strict-aliasing")

add_definitions(-DOS_LINUX)

# Set CPU
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86")
  add_definitions(-DARCH_X86)
endif()

include_directories(
  ${PROJECT_ROOT_DIR}/include
  ${PROJECT_ROOT_DIR}/src
  ${MLAB_LIBRARIES_ROOT}/include
  ${GTEST_ROOT}/include)

link_directories(
  ${MLAB_LIBRARIES_ROOT}/lib
  ${GTEST_ROOT}/lib)
 
file(GLOB_RECURSE SRC_FILES src/*.cc)

# Build the libraries
# Nothing

# Build 
add_executable(mping ${SRC_FILES})
target_link_libraries(mping mlab)

# Build supplement targets
add_subdirectory(test)
#add_subdirectory(${MLAB_LIBRARIES_ROOT})
#add_subdirectory(${GTEST_ROOT})
