cmake_minimum_required(VERSION 3.16)
project(parser-generator)

enable_testing()
set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall)
include(../cmake/IWYU.cmake OPTIONAL)

include_directories(include)
include_directories(runtime-include)
aux_source_directory(src SRC)
remove(SRC "src/Main.cpp")
add_library(${PROJECT_NAME}-lib ${SRC})
target_precompile_headers(
  ${PROJECT_NAME}-lib
  PRIVATE
  include/LLTable.hpp
  include/Utility.hpp
)

add_executable(${PROJECT_NAME} "src/Main.cpp")
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-lib)

if (${${PROJECT_NAME}_DISABLE_TESTS}) 
  return() 
endif()
aux_source_directory(test TEST_SRC)
if(TEST_SRC)
  create_test_sourcelist(
    TESTS
    CommonTest.cpp
    ${TEST_SRC}
  )
  add_executable(${PROJECT_NAME}-test ${TESTS})
  remove(TESTS CommonTest.cpp)
  target_link_libraries(${PROJECT_NAME}-test ${PROJECT_NAME}-lib)
  foreach(TEST_FILE ${TESTS})
    cmake_path(GET TEST_FILE STEM TEST_NAME)
    add_test(NAME test/${TEST_NAME} COMMAND ${PROJECT_NAME}-test test/${TEST_NAME})
  endforeach()
endif()
