function(create_gtest dir_name lib_target)
  find_package(GTest)
  if (GTest_FOUND)
    aux_source_directory(${dir_name} TEST_SRC)
    add_executable(${PROJECT_NAME}-test ${TEST_SRC})
    target_link_libraries(${PROJECT_NAME}-test ${lib_target})
    target_include_directories(${PROJECT_NAME}-test PUBLIC $<TARGET_PROPERTY:${lib_target},INCLUDE_DIRECTORIES>)
    target_precompile_headers(${PROJECT_NAME}-test REUSE_FROM ${lib_target})
    include(GoogleTest)
    target_link_libraries(
      ${PROJECT_NAME}-test
      GTest::gtest_main
    )
    gtest_discover_tests(${PROJECT_NAME}-test)
  else()
    message("${PROJECT_NAME} skip testing because GTest is not found")
  endif()
endfunction()
