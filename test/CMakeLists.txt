set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL TRUE)
include_directories(${PROJECT_SOURCE_DIR}/include)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMP_TEST")

file(GLOB_RECURSE TEST_SRC_FILES *_test.cc ${PROJECT_SOURCE_DIR}/src/mp_*.cc)
add_executable(mping_test ${TEST_SRC_FILES})

target_link_libraries(mping_test 
  gtest_main
  mlab)
