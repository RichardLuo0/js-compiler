cmake_minimum_required(VERSION 3.23)
project(js-compiler)

enable_testing()
set(CMAKE_CXX_STANDARD 17)

# Generate parser
add_custom_target(
   parser-generator
   COMMAND cmake .
   WORKING_DIRECTORY parser-generator
)
add_custom_command(OUTPUT src/LLTable.cpp
 COMMAND parser-generator/build/debug/parser-generator js.ebnf -o src/LLTable.cpp
 MAIN_DEPENDENCY parser-generator
 DEPENDS parser-generator/build/debug/parser-generator js.ebnf
)
add_custom_target(lltableSRC DEPENDS src/LLTable.cpp)
include_directories(lltable-generator/template)

# Link LLVM
execute_process(COMMAND llvm-config --libs core engine nativecodegen x86asmparser mcjit passes OUTPUT_VARIABLE LLVM_LIBS)
string(STRIP ${LLVM_LIBS} LLVM_LIBS)

# Generate lib
include_directories(include)
aux_source_directory(src SRC)
remove(SRC src/Main.cpp)
add_library(lib ${SRC})
add_dependencies(lib lltableSRC)
target_link_libraries(lib ${LLVM_LIBS})

add_executable(${PROJECT_NAME} "src/Main.cpp")
target_link_libraries(${PROJECT_NAME} lib)
include(CPack)

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
