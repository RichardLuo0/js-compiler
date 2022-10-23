cmake_minimum_required(VERSION 3.19)
project(parser-generator)

enable_testing()
set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wno-trigraphs -Wfatal-errors)
add_link_options(-fuse-ld=lld)
set(${PROJECT_NAME}_ENTRY "src/Main.cpp")

aux_source_directory(src ${PROJECT_NAME}-SRC)
remove(${PROJECT_NAME}-SRC ${${PROJECT_NAME}_ENTRY})
add_library(${PROJECT_NAME}-lib ${${PROJECT_NAME}-SRC})
target_precompile_headers(
  ${PROJECT_NAME}-lib
  PRIVATE
  include/Utility.hpp
)
target_include_directories(${PROJECT_NAME}-lib PRIVATE include)
target_include_directories(${PROJECT_NAME}-lib PUBLIC generated-parser)

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_ENTRY})
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}-lib)
target_include_directories(${PROJECT_NAME} PRIVATE $<TARGET_PROPERTY:${PROJECT_NAME}-lib,INCLUDE_DIRECTORIES>)

# GTest
if (${${PROJECT_NAME}_DISABLE_TESTS})
  message("Testing is disabled in ${PROJECT_NAME} by user" )
else()
  message("Enabled Testing in ${PROJECT_NAME}")
  include(../cmake/GTest.cmake OPTIONAL RESULT_VARIABLE IS_GTEST_FOUND)
  if (NOT IS_GTEST_FOUND STREQUAL "NOTFOUND")
    create_gtest(unitTest ${PROJECT_NAME}-lib)
  endif()
endif()
