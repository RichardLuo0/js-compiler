find_package(GTest)
if (GTest_FOUND)
  aux_source_directory(unitTest TEST_SRC)
  add_executable(${PROJECT_NAME}-test ${TEST_SRC})
  target_link_libraries(${PROJECT_NAME}-test ${PROJECT_NAME}-lib)
  include(GoogleTest)
  target_link_libraries(
    ${PROJECT_NAME}-test
    GTest::gtest_main
  )
  gtest_discover_tests(${PROJECT_NAME}-test)
else()
  message("Skip testing because GTest is no found")
endif()
