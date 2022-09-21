cmake_minimum_required(VERSION 3.0)
project(js-compiler)

enable_testing()
set(CMAKE_CXX_STANDARD 17)
add_compile_options(-Wall)
include(cmake/IWYU.cmake)

# Generate parser
set(${parser-generator_DISABLE_TESTS} false)
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
if (NOT LLVM_LIBS)
  message(FATAL_ERROR "No llvm found in path")
endif()
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
