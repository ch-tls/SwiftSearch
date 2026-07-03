add_executable(swiftsearch_test_runner
  test_main.cpp
  test_index_engine.cpp
  test_search_service.cpp
  test_timsort.cpp
  test_platform_scanner.cpp
  test_observer_pattern.cpp
  test_command_pattern.cpp
  test_logging.cpp
)
target_include_directories(swiftsearch_test_runner PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
)
target_link_libraries(swiftsearch_test_runner PRIVATE
  swiftsearch_model
  swiftsearch_controller
  swiftsearch_util
  Qt6::Core
  Qt6::Sql
  GTest::gtest
  GTest::gtest_main
)

swiftsearch_enforce_warnings(swiftsearch_test_runner)

include(GoogleTest)
gtest_discover_tests(swiftsearch_test_runner)
