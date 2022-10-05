cmake_minimum_required(VERSION 3.16)
project(parser-generator)

enable_testing()
set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wfatal-errors)
add_link_options(-fuse-ld=lld)
set(${PROJECT_NAME}_ENTRY "src/Main.cpp")
include(../cmake/IWYU.cmake OPTIONAL)

include_directories(include)
include_directories(generated-parser)
aux_source_directory(src SRC)
remove(SRC ${${PROJECT_NAME}_ENTRY})
add_library(${PROJECT_NAME}-lib ${SRC})
target_precompile_headers(
  ${PROJECT_NAME}-lib
  PRIVATE
  include/LLTable.hpp
)

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_ENTRY})
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-lib)

# GTest
if (${${PROJECT_NAME}_DISABLE_TESTS})
  message("Testing is disabled in ${PROJECT_NAME} by user" )
else()
  message("Enabled Testing")
  include(../cmake/GTest.cmake OPTIONAL)
endif()
