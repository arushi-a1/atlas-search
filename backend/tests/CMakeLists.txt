add_executable(dse_tests
  test_main.cc
  test_tokenizer_robots.cc
  test_index_bm25.cc
  test_html_parser.cc
)
target_link_libraries(dse_tests dse_core)
target_include_directories(dse_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME dse_tests COMMAND dse_tests)
