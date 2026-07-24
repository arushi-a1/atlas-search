// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT
//
// A politeness-aware crawler: per-host robots.txt caching, per-host crawl
// delay, and a URL frontier. Designed to run as a standalone worker
// process (see worker/) so many of these can run in parallel behind a
// shared frontier (Redis-backed queue, see docs/ARCHITECTURE.md).

#ifndef DISTRIBUTED_SEARCH_CRAWLER_CRAWLER_H_
#define DISTRIBUTED_SEARCH_CRAWLER_CRAWLER_H_

#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "crawler/robots.h"
#include "parser/html_parser.h"

namespace dse {
namespace crawler {

struct CrawledDocument {
  std::string url;
  std::string title;
  std::string text;
  std::vector<std::string> headings;
  std::vector<std::string> discovered_links;
};

struct CrawlConfig {
  std::string user_agent = "DistributedSearchBot/1.0 (+https://example.com/bot)";
  int max_pages = 1000;
  int max_depth = 4;
  std::chrono::milliseconds default_crawl_delay{500};
  std::chrono::milliseconds fetch_timeout{5000};
};

// Called once per successfully-crawled page. Return value lets the caller
// push the document into the index / a downstream queue.
using DocumentCallback = std::function<void(const CrawledDocument&)>;

class Crawler {
 public:
  explicit Crawler(CrawlConfig config);

  void SeedUrls(const std::vector<std::string>& urls);

  // Runs a synchronous BFS crawl on the calling thread up to
  // config.max_pages, invoking on_document for each page. In the
  // distributed deployment, N of these run in separate worker processes,
  // each pulling from a shared Redis-backed frontier instead of the local
  // deque (see worker/README.md).
  void Run(const DocumentCallback& on_document);

 private:
  struct FrontierEntry {
    std::string url;
    int depth;
  };

  const RobotsPolicy& GetRobotsPolicy(const std::string& scheme,
                                       const std::string& host, int port);
  static bool SplitUrl(const std::string& url, std::string* scheme,
                        std::string* host, int* port, std::string* path);

  CrawlConfig config_;
  std::deque<FrontierEntry> frontier_;
  std::unordered_set<std::string> visited_;
  std::unordered_map<std::string, RobotsPolicy> robots_cache_;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point>
      last_fetch_by_host_;
};

}  // namespace crawler
}  // namespace dse

#endif  // DISTRIBUTED_SEARCH_CRAWLER_CRAWLER_H_
