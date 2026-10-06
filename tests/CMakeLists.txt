find_package(GTest REQUIRED)

add_executable(botuvera_tests
  http_request_parse_test.cpp
  session_e2e_test.cpp
)

target_link_libraries(botuvera_tests PRIVATE
  botuvera_lib
  GTest::gtest_main
)

add_test(NAME botuvera_tests COMMAND botuvera_tests)
