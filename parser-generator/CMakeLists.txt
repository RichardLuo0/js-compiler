cmake_minimum_required(VERSION 3.23)
project(parser-generator)

enable_testing()
set(CMAKE_CXX_STANDARD 17)

include_directories(include)
include_directories(template)
aux_source_directory(src SRC)
add_library(lib ${SRC})

add_executable(${PROJECT_NAME} "src/Main.cpp")
target_link_libraries(${PROJECT_NAME} lib)

aux_source_directory(test TEST_SRC)
create_test_sourcelist(TESTS
  CommonTest.cpp
  ${TEST_SRC}
)
add_executable(CommonTest ${TESTS})
remove(TESTS CommonTest.cpp)
target_link_libraries(CommonTest lib)
foreach(TEST_FILE ${TESTS})
  cmake_path(GET TEST_FILE STEM TEST_NAME)
  add_test(NAME test/${TEST_NAME} COMMAND CommonTest test/${TEST_NAME})
endforeach()
