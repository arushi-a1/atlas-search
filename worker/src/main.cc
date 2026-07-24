// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT
//
// Entrypoint for a crawl worker node. Each worker is an independent
// process/container that claims a shard of seed URLs (by WORKER_INDEX /
// WORKER_COUNT, hashing hosts to shards so a given host is only ever
// crawled by one worker at a time -- keeping per-host politeness state
// consistent without cross-worker coordination) and writes crawled
// documents to a shared output the indexer can pick up.
//
// This is intentionally simple: for a real distributed deployment, swap
// the static seed file + local output file for a Redis-backed frontier
// queue (BRPOPLPUSH for claim-and-work-with-visibility-timeout semantics)
// and a message bus (or the indexer's own ingestion API) for output. The
// crawler/indexing logic itself doesn't change -- only where the frontier
// and sink live. See docs/ARCHITECTURE.md for the target topology.

#include <cstdlib>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "common/logger.h"
#include "crawler/crawler.h"

namespace {

std::string GetEnv(const char* name, const std::string& fallback) {
  const char* val = std::getenv(name);
  return val != nullptr ? std::string(val) : fallback;
}

std::vector<std::string> LoadSeeds(const std::string& path) {
  std::vector<std::string> seeds;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty()) seeds.push_back(line);
  }
  return seeds;
}

// Simple string hash to deterministically shard hosts across workers.
size_t HashHost(const std::string& url) {
  return std::hash<std::string>{}(url);
}

}  // namespace

int main() {
  int worker_index = std::atoi(GetEnv("WORKER_INDEX", "0").c_str());
  int worker_count = std::max(1, std::atoi(GetEnv("WORKER_COUNT", "1").c_str()));
  std::string seed_file = GetEnv("DSE_SEED_FILE", "/data/seeds.txt");
  std::string output_file = GetEnv("DSE_OUTPUT_FILE", "/data/documents.tsv");
  int max_pages = std::atoi(GetEnv("DSE_MAX_PAGES", "500").c_str());

  std::vector<std::string> all_seeds = LoadSeeds(seed_file);
  std::vector<std::string> my_seeds;
  for (const auto& url : all_seeds) {
    if (static_cast<int>(HashHost(url) % worker_count) == worker_index) {
      my_seeds.push_back(url);
    }
  }

  DSE_LOG_INFO("worker", "worker " + std::to_string(worker_index) + "/" +
                              std::to_string(worker_count) + " claimed " +
                              std::to_string(my_seeds.size()) + " seed(s)");

  dse::crawler::CrawlConfig config;
  config.max_pages = max_pages;

  dse::crawler::Crawler crawler(config);
  crawler.SeedUrls(my_seeds);

  std::ofstream out(output_file, std::ios::app);
  std::mutex out_mutex;

  crawler.Run([&](const dse::crawler::CrawledDocument& doc) {
    std::lock_guard<std::mutex> lock(out_mutex);
    std::string sanitize_tabs = doc.title;
    for (auto& c : sanitize_tabs) if (c == '\t' || c == '\n') c = ' ';
    std::string body = doc.text;
    for (auto& c : body) if (c == '\t' || c == '\n') c = ' ';
    out << doc.url << "\t" << sanitize_tabs << "\t" << body << "\n";
  });

  return 0;
}
