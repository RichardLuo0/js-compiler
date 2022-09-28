cmake_minimum_required(VERSION 3.16)
project(js-compiler)

enable_testing()
set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall)
include(cmake/IWYU.cmake)

# Generate parser
set(parser-generator_DISABLE_TESTS TRUE)
add_subdirectory(parser-generator EXCLUDE_FROM_ALL)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Parser.cpp
  COMMAND parser-generator bnf/js.ebnf -o ${CMAKE_CURRENT_BINARY_DIR}/Parser.cpp
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  MAIN_DEPENDENCY parser-generator
  DEPENDS bnf/js.ebnf
)
aux_source_directory(parser-generator/runtime-include PASER_GENERATOR_HEADER)
include_directories(parser-generator/runtime-include)
file(GLOB PARSER_GENERATOR_RUNTIME parser-generator/runtime-include/*.hpp)

# Link LLVM
execute_process(COMMAND llvm-config --libs core OUTPUT_VARIABLE LLVM_LIBS)
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
target_precompile_headers(
  ${PROJECT_NAME}-lib 
  PRIVATE
  ${PARSER_GENERATOR_RUNTIME} 
  include/Exception.hpp
  include/Utility.hpp
)

# Generate executable
add_executable(${PROJECT_NAME} "src/Main.cpp")
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-lib)
include(CPack)

# GTest
include(cmake/GTest.cmake)
