// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#ifndef DISTRIBUTED_SEARCH_API_SEARCH_API_H_
#define DISTRIBUTED_SEARCH_API_SEARCH_API_H_

#include <atomic>
#include <memory>

#include "cache/redis_client.h"
#include "http/http_server.h"
#include "index/bm25.h"
#include "index/inverted_index.h"

namespace dse {
namespace api {

class SearchApi {
 public:
  SearchApi(index::InvertedIndex* index, cache::RedisClient* cache);

  // Registers /api/search, /api/stats, /health onto the server.
  void RegisterRoutes(http::HttpServer* server);

 private:
  http::HttpResponse HandleSearch(const http::HttpRequest& request);
  http::HttpResponse HandleStats(const http::HttpRequest& request);
  http::HttpResponse HandleHealth(const http::HttpRequest& request);
  http::HttpResponse HandleMetrics(const http::HttpRequest& request);

  index::InvertedIndex* index_;
  cache::RedisClient* cache_;  // may be nullptr if caching is disabled
  index::Bm25Ranker ranker_;

  std::atomic<uint64_t> search_requests_total_{0};
  std::atomic<uint64_t> cache_hits_total_{0};
  std::atomic<uint64_t> cache_misses_total_{0};
};

}  // namespace api
}  // namespace dse

#endif  // DISTRIBUTED_SEARCH_API_SEARCH_API_H_
