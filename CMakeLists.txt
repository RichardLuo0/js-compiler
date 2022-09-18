cmake_minimum_required(VERSION 3.23)
project(js-compiler)

enable_testing()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_DEBUG "-Wall ${CMAKE_CXX_FLAGS_DEBUG}")
include(cmake/IWYU.cmake)

# Generate parser
add_subdirectory(parser-generator EXCLUDE_FROM_ALL)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Parser.cpp
  COMMAND parser-generator bnf/js.ebnf -o ${CMAKE_CURRENT_BINARY_DIR}/Parser.cpp
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  MAIN_DEPENDENCY parser-generator
  DEPENDS bnf/js.ebnf
)
include_directories(parser-generator/runtime-include)

# Link LLVM
execute_process(COMMAND llvm-config --libs core engine nativecodegen x86asmparser mcjit passes OUTPUT_VARIABLE LLVM_LIBS)
string(STRIP ${LLVM_LIBS} LLVM_LIBS)

# Generate lib
include_directories(include)
aux_source_directory(src SRC)
remove(SRC "src/Main.cpp")
list(APPEND SRC ${CMAKE_CURRENT_BINARY_DIR}/Parser.cpp)
add_library(${PROJECT_NAME}-lib ${SRC})
target_link_libraries(${PROJECT_NAME}-lib ${LLVM_LIBS})

add_executable(${PROJECT_NAME} "src/Main.cpp")
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-lib)
include(CPack)

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
