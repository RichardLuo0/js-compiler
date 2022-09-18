cmake_minimum_required(VERSION 3.23)
project(parser-generator)

enable_testing()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_DEBUG "-Wall ${CMAKE_CXX_FLAGS_DEBUG}")
include(../cmake/IWYU.cmake OPTIONAL)

include_directories(include)
include_directories(runtime-include)
aux_source_directory(src SRC)
remove(SRC "src/Main.cpp")
add_library(${PROJECT_NAME}-lib ${SRC})

add_executable(${PROJECT_NAME} "src/Main.cpp")
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-lib)

aux_source_directory(test TEST_SRC)
if(${TEST_SRC})
  create_test_sourcelist(
    TESTS
    CommonTest.cpp
    ${TEST_SRC}
  )
  add_executable(${PROJECT_NAME}-CommonTest ${TESTS})
  remove(TESTS CommonTest.cpp)
  target_link_libraries(${PROJECT_NAME}-CommonTest ${PROJECT_NAME}-lib)
  foreach(TEST_FILE ${TESTS})
    cmake_path(GET TEST_FILE STEM TEST_NAME)
    add_test(NAME test/${TEST_NAME} COMMAND ${PROJECT_NAME}-CommonTest test/${TEST_NAME})
  endforeach()
endif()
