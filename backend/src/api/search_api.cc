// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#include "api/search_api.h"

#include <sstream>

#include "common/json.h"
#include "common/logger.h"

namespace dse {
namespace api {

using common::JsonEscape;
using http::HttpRequest;
using http::HttpResponse;

SearchApi::SearchApi(index::InvertedIndex* index, cache::RedisClient* cache)
    : index_(index), cache_(cache), ranker_(*index) {}

void SearchApi::RegisterRoutes(http::HttpServer* server) {
  server->Get("/api/search", [this](const HttpRequest& r) { return HandleSearch(r); });
  server->Get("/api/stats", [this](const HttpRequest& r) { return HandleStats(r); });
  server->Get("/health", [this](const HttpRequest& r) { return HandleHealth(r); });
  server->Get("/metrics", [this](const HttpRequest& r) { return HandleMetrics(r); });
}

HttpResponse SearchApi::HandleSearch(const HttpRequest& request) {
  search_requests_total_.fetch_add(1, std::memory_order_relaxed);
  auto it = request.query_params.find("q");
  if (it == request.query_params.end() || it->second.empty()) {
    return HttpResponse::Json(400, R"({"error":"missing required query param 'q'"})");
  }
  std::string query = it->second;

  size_t top_k = 10;
  auto k_it = request.query_params.find("k");
  if (k_it != request.query_params.end()) {
    try { top_k = std::min<size_t>(100, std::stoul(k_it->second)); } catch (...) {}
  }

  std::string cache_key = "search:" + query + ":" + std::to_string(top_k);
  if (cache_ != nullptr) {
    auto cached = cache_->Get(cache_key);
    if (cached.has_value()) {
      cache_hits_total_.fetch_add(1, std::memory_order_relaxed);
      HttpResponse resp = HttpResponse::Json(200, *cached);
      resp.headers["X-Cache"] = "HIT";
      return resp;
    }
    cache_misses_total_.fetch_add(1, std::memory_order_relaxed);
  }

  std::vector<index::SearchResult> results = ranker_.Search(query, top_k);

  std::ostringstream body;
  body << R"({"query":")" << JsonEscape(query) << R"(","count":)" << results.size()
       << R"(,"results":[)";
  for (size_t i = 0; i < results.size(); ++i) {
    const auto& r = results[i];
    if (i > 0) body << ",";
    body << "{"
         << R"("url":")" << JsonEscape(r.url) << R"(",)"
         << R"("title":")" << JsonEscape(r.title) << R"(",)"
         << R"("snippet":")" << JsonEscape(r.snippet) << R"(",)"
         << R"("score":)" << r.score << "}";
  }
  body << "]}";
  std::string json_body = body.str();

  if (cache_ != nullptr) {
    cache_->Set(cache_key, json_body, 300 /* 5 min TTL */);
  }

  HttpResponse resp = HttpResponse::Json(200, json_body);
  resp.headers["X-Cache"] = "MISS";
  return resp;
}

HttpResponse SearchApi::HandleStats(const HttpRequest&) {
  std::ostringstream body;
  body << "{"
       << R"("document_count":)" << index_->DocumentCount() << ","
       << R"("average_body_length":)" << index_->AverageBodyLength() << ","
       << R"("cache_connected":)" << (cache_ != nullptr && cache_->IsConnected() ? "true" : "false")
       << "}";
  return HttpResponse::Json(200, body.str());
}

HttpResponse SearchApi::HandleHealth(const HttpRequest&) {
  return HttpResponse::Json(200, R"({"status":"ok"})");
}

HttpResponse SearchApi::HandleMetrics(const HttpRequest&) {
  std::ostringstream out;
  out << "# HELP dse_documents_indexed Total documents in the in-memory index\n"
      << "# TYPE dse_documents_indexed gauge\n"
      << "dse_documents_indexed " << index_->DocumentCount() << "\n"
      << "# HELP dse_search_requests_total Total /api/search requests served\n"
      << "# TYPE dse_search_requests_total counter\n"
      << "dse_search_requests_total " << search_requests_total_.load() << "\n"
      << "# HELP dse_cache_hits_total Result-cache hits\n"
      << "# TYPE dse_cache_hits_total counter\n"
      << "dse_cache_hits_total " << cache_hits_total_.load() << "\n"
      << "# HELP dse_cache_misses_total Result-cache misses\n"
      << "# TYPE dse_cache_misses_total counter\n"
      << "dse_cache_misses_total " << cache_misses_total_.load() << "\n"
      << "# HELP dse_cache_connected Whether the Redis cache connection is up\n"
      << "# TYPE dse_cache_connected gauge\n"
      << "dse_cache_connected " << (cache_ != nullptr && cache_->IsConnected() ? 1 : 0) << "\n";

  HttpResponse resp;
  resp.status_code = 200;
  resp.status_text = "OK";
  resp.headers["Content-Type"] = "text/plain; version=0.0.4";
  resp.body = out.str();
  return resp;
}

}  // namespace api
}  // namespace dse
