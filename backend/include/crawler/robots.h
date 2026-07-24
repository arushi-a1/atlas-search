// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT
//
// Parses robots.txt (RFC 9309-ish subset) for a single user-agent group:
// Allow/Disallow rules with longest-match-wins precedence, plus
// Crawl-delay and Sitemap directives.

#ifndef DISTRIBUTED_SEARCH_CRAWLER_ROBOTS_H_
#define DISTRIBUTED_SEARCH_CRAWLER_ROBOTS_H_

#include <chrono>
#include <string>
#include <vector>

namespace dse {
namespace crawler {

class RobotsPolicy {
 public:
  // Parses robots.txt content, filtering rules that apply to user_agent
  // (falls back to "*" if no specific group matches).
  static RobotsPolicy Parse(const std::string& content,
                             const std::string& user_agent);

  bool IsAllowed(const std::string& path) const;
  std::chrono::milliseconds CrawlDelay() const { return crawl_delay_; }
  const std::vector<std::string>& Sitemaps() const { return sitemaps_; }

 private:
  struct Rule {
    std::string pattern;
    bool allow;
  };
  std::vector<Rule> rules_;
  std::chrono::milliseconds crawl_delay_{0};
  std::vector<std::string> sitemaps_;
};

}  // namespace crawler
}  // namespace dse

#endif  // DISTRIBUTED_SEARCH_CRAWLER_ROBOTS_H_
