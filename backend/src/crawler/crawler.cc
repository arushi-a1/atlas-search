// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#include "crawler/crawler.h"

#include <regex>
#include <thread>

#include "common/logger.h"
#include "crawler/http_client.h"

namespace dse {
namespace crawler {

Crawler::Crawler(CrawlConfig config) : config_(std::move(config)) {}

void Crawler::SeedUrls(const std::vector<std::string>& urls) {
  for (const auto& url : urls) {
    frontier_.push_back({url, 0});
  }
}

bool Crawler::SplitUrl(const std::string& url, std::string* scheme,
                        std::string* host, int* port, std::string* path) {
  static const std::regex kUrlRe(
      R"(^(https?)://([^/:]+)(?::(\d+))?(/.*)?$)", std::regex::icase);
  std::smatch m;
  if (!std::regex_match(url, m, kUrlRe)) return false;
  *scheme = m[1].str();
  *host = m[2].str();
  *port = m[3].matched ? std::stoi(m[3].str()) : (*scheme == "https" ? 443 : 80);
  *path = m[4].matched ? m[4].str() : "/";
  return true;
}

const RobotsPolicy& Crawler::GetRobotsPolicy(const std::string& scheme,
                                              const std::string& host,
                                              int port) {
  std::string key = scheme + "://" + host + ":" + std::to_string(port);
  auto it = robots_cache_.find(key);
  if (it != robots_cache_.end()) return it->second;

  // NOTE: this client is plain-TCP; for https hosts a production
  // deployment should fetch robots.txt via a TLS-capable client (libcurl).
  // Here we still attempt the request so http hosts and test fixtures work
  // end-to-end, and fail open (treat as "no robots.txt found" -> allow
  // all) for anything we can't fetch, matching common crawler behavior.
  FetchResult result = HttpClient::Get(host, port, "/robots.txt",
                                        config_.user_agent, std::chrono::milliseconds(3000));
  RobotsPolicy policy;
  if (result.success) {
    policy = RobotsPolicy::Parse(result.body, config_.user_agent);
  }
  auto [inserted, _] = robots_cache_.emplace(key, std::move(policy));
  return inserted->second;
}

void Crawler::Run(const DocumentCallback& on_document) {
  int fetched = 0;
  while (!frontier_.empty() && fetched < config_.max_pages) {
    FrontierEntry entry = frontier_.front();
    frontier_.pop_front();

    if (visited_.count(entry.url)) continue;
    visited_.insert(entry.url);
    if (entry.depth > config_.max_depth) continue;

    std::string scheme, host, path;
    int port;
    if (!SplitUrl(entry.url, &scheme, &host, &port, &path)) continue;

    const RobotsPolicy& policy = GetRobotsPolicy(scheme, host, port);
    if (!policy.IsAllowed(path)) {
      DSE_LOG_DEBUG("crawler", "robots.txt disallows " + entry.url);
      continue;
    }

    // Politeness: respect crawl-delay (or the configured default) per host.
    auto delay = policy.CrawlDelay().count() > 0 ? policy.CrawlDelay()
                                                  : config_.default_crawl_delay;
    auto last_it = last_fetch_by_host_.find(host);
    if (last_it != last_fetch_by_host_.end()) {
      auto elapsed = std::chrono::steady_clock::now() - last_it->second;
      if (elapsed < delay) {
        std::this_thread::sleep_for(delay - elapsed);
      }
    }

    FetchResult result = HttpClient::Get(host, port, path, config_.user_agent,
                                          config_.fetch_timeout);
    last_fetch_by_host_[host] = std::chrono::steady_clock::now();

    if (!result.success) {
      DSE_LOG_WARN("crawler", "fetch failed: " + entry.url + " (" + result.error + ")");
      continue;
    }
    if (result.content_type.find("text/html") == std::string::npos &&
        !result.content_type.empty()) {
      continue;  // Skip non-HTML content (images, PDFs handled by a
                 // separate extractor pipeline in production).
    }

    parser::ParsedPage parsed = parser::HtmlParser::Parse(result.body, entry.url);

    CrawledDocument doc;
    doc.url = entry.url;
    doc.title = parsed.title;
    doc.text = parsed.visible_text;
    doc.headings = parsed.headings;
    doc.discovered_links = parsed.links;
    on_document(doc);
    ++fetched;

    for (const auto& link : parsed.links) {
      if (!visited_.count(link)) {
        frontier_.push_back({link, entry.depth + 1});
      }
    }
  }
  DSE_LOG_INFO("crawler", "crawl finished: " + std::to_string(fetched) + " pages fetched");
}

}  // namespace crawler
}  // namespace dse
