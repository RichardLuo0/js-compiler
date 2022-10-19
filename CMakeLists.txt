cmake_minimum_required(VERSION 3.19)
project(js-compiler)

enable_testing()
set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wno-trigraphs -Wfatal-errors)
add_link_options(-fuse-ld=lld)
set(${PROJECT_NAME}_ENTRY "src/Main.cpp")
include(cmake/IWYU.cmake)

# Generate lib
aux_source_directory(src ${PROJECT_NAME}-SRC)
remove(${PROJECT_NAME}-SRC ${${PROJECT_NAME}_ENTRY})
add_library(${PROJECT_NAME}-lib ${${PROJECT_NAME}-SRC})
target_include_directories(${PROJECT_NAME}-lib PUBLIC include)
target_precompile_headers(
  ${PROJECT_NAME}-lib
  PRIVATE
  include/Exception.hpp
  include/Utility.hpp
)

# Generate executable
add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_ENTRY})
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-lib)
target_precompile_headers(${PROJECT_NAME} REUSE_FROM ${PROJECT_NAME}-lib)
target_include_directories(${PROJECT_NAME} PUBLIC $<TARGET_PROPERTY:${PROJECT_NAME}-lib,INCLUDE_DIRECTORIES>)

# Generate parser
set(parser-generator_DISABLE_TESTS false)
add_subdirectory(parser-generator EXCLUDE_FROM_ALL)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/js.ebnf.bin
  COMMAND parser-generator bnf/js.ebnf -o ${CMAKE_CURRENT_BINARY_DIR}/js.ebnf.bin
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  MAIN_DEPENDENCY parser-generator
  DEPENDS bnf/js.ebnf
)
include(cmake/bin2c.cmake)
bin2c(${PROJECT_NAME}-lib ${CMAKE_CURRENT_BINARY_DIR}/js.ebnf.bin js_ebnf)
file(GLOB GENERATED_PARSER_HEADER parser-generator/generated-parser/*.hpp)
target_include_directories(${PROJECT_NAME}-lib PRIVATE parser-generator/generated-parser)
target_precompile_headers(${PROJECT_NAME}-lib PRIVATE ${GENERATED_PARSER_HEADER})

# Link LLVM
execute_process(COMMAND llvm-config --libs core OUTPUT_VARIABLE LLVM_LIBS)
if (NOT LLVM_LIBS)
  message(FATAL_ERROR "llvm is not found in path")
endif()
string(STRIP ${LLVM_LIBS} LLVM_LIBS)
target_link_libraries(${PROJECT_NAME}-lib ${LLVM_LIBS})

include(CPack)

# GTest
include(cmake/GTest.cmake)
create_gtest(unitTest ${PROJECT_NAME}-lib)
