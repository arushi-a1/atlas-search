cmake_minimum_required(VERSION 3.16)
project(distributed_search_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -pthread)
link_libraries(pthread)

include_directories(include)

# ---- Core library: everything except the two main()s ----
add_library(dse_core
  src/common/thread_pool.cc
  src/http/router.cc
  src/http/http_server.cc
  src/parser/html_parser.cc
  src/crawler/robots.cc
  src/crawler/http_client.cc
  src/crawler/crawler.cc
  src/index/tokenizer.cc
  src/index/inverted_index.cc
  src/index/bm25.cc
  src/cache/redis_client.cc
  src/api/search_api.cc
)
target_include_directories(dse_core PUBLIC include)

# ---- API server binary ----
add_executable(search_server src/main.cc)
target_link_libraries(search_server dse_core)

# ---- Crawl worker binary ----
add_executable(crawl_worker ../worker/src/main.cc)
target_link_libraries(crawl_worker dse_core)

# ---- Tests ----
enable_testing()
add_subdirectory(tests)
