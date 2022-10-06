cmake_minimum_required(VERSION 3.16)
project(js-compiler)

enable_testing()
set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wno-trigraphs -Wfatal-errors)
add_link_options(-fuse-ld=lld)
set(${PROJECT_NAME}_ENTRY "src/Main.cpp")
include(cmake/IWYU.cmake)

# Generate parser
set(parser-generator_DISABLE_TESTS true)
add_subdirectory(parser-generator EXCLUDE_FROM_ALL)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Parser.cpp
  COMMAND parser-generator bnf/js.ebnf -o ${CMAKE_CURRENT_BINARY_DIR}/Parser.cpp
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  MAIN_DEPENDENCY parser-generator
  DEPENDS bnf/js.ebnf
)
include_directories(parser-generator/generated-parser)
file(GLOB GENERATED_PARSER_HEADER parser-generator/runtime-include/*.hpp)

# Link LLVM
execute_process(COMMAND llvm-config --libs core OUTPUT_VARIABLE LLVM_LIBS)
if (NOT LLVM_LIBS)
  message(FATAL_ERROR "No llvm found in path")
endif()
string(STRIP ${LLVM_LIBS} LLVM_LIBS)

# Generate lib
include_directories(include)
aux_source_directory(src SRC)
remove(SRC ${${PROJECT_NAME}_ENTRY})
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
add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_ENTRY})
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-lib)
target_precompile_headers(${PROJECT_NAME} REUSE_FROM ${PROJECT_NAME}-lib)

include(CPack)

# GTest
include(cmake/GTest.cmake)
