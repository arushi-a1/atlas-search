// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT
//
// Entrypoint for the query-serving API node. Loads a crawled/indexed
// document dump (see worker/), stands up the inverted index in memory,
// connects to Redis for result caching, and serves the REST API.

#include <csignal>
#include <cstdlib>
#include <fstream>
#include <memory>

#include "api/search_api.h"
#include "cache/redis_client.h"
#include "common/logger.h"
#include "http/http_server.h"
#include "index/inverted_index.h"

namespace {
dse::http::HttpServer* g_server = nullptr;

void HandleSignal(int) {
  if (g_server != nullptr) g_server->Stop();
}

std::string GetEnv(const char* name, const std::string& fallback) {
  const char* val = std::getenv(name);
  return val != nullptr ? std::string(val) : fallback;
}

// Loads newline-delimited JSON-ish document dumps produced by worker/.
// Format: one doc per line as TSV: url \t title \t body_text
// (kept intentionally simple -- swap for a real JSON/protobuf ingestion
// pipeline in production; see docs/ARCHITECTURE.md).
void LoadDocuments(const std::string& path, dse::index::InvertedIndex* index) {
  std::ifstream in(path);
  if (!in.is_open()) {
    DSE_LOG_WARN("main", "no document dump found at " + path + "; starting with an empty index");
    return;
  }
  std::string line;
  size_t loaded = 0;
  while (std::getline(in, line)) {
    size_t t1 = line.find('\t');
    if (t1 == std::string::npos) continue;
    size_t t2 = line.find('\t', t1 + 1);
    if (t2 == std::string::npos) continue;
    dse::index::IndexedDocument doc;
    doc.url = line.substr(0, t1);
    doc.title = line.substr(t1 + 1, t2 - t1 - 1);
    doc.body_text = line.substr(t2 + 1);
    index->AddDocument(doc);
    ++loaded;
  }
  DSE_LOG_INFO("main", "loaded " + std::to_string(loaded) + " documents from " + path);
}

}  // namespace

int main() {
  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);

  std::string bind_address = GetEnv("DSE_BIND_ADDRESS", "0.0.0.0");
  int port = std::atoi(GetEnv("DSE_PORT", "8080").c_str());
  size_t threads = static_cast<size_t>(std::atoi(GetEnv("DSE_THREADS", "16").c_str()));
  std::string redis_host = GetEnv("DSE_REDIS_HOST", "");
  int redis_port = std::atoi(GetEnv("DSE_REDIS_PORT", "6379").c_str());
  std::string doc_dump = GetEnv("DSE_DOCUMENT_DUMP", "/data/documents.tsv");

  dse::index::InvertedIndex index;
  LoadDocuments(doc_dump, &index);

  std::unique_ptr<dse::cache::RedisClient> redis;
  if (!redis_host.empty()) {
    redis = std::make_unique<dse::cache::RedisClient>(redis_host, redis_port);
    if (!redis->Connect()) {
      DSE_LOG_WARN("main", "could not connect to redis at " + redis_host +
                                "; continuing without caching");
      redis.reset();
    }
  }

  dse::http::HttpServer server(bind_address, port, threads);
  dse::api::SearchApi api(&index, redis.get());
  api.RegisterRoutes(&server);

  g_server = &server;
  server.Run();
  return 0;
}
