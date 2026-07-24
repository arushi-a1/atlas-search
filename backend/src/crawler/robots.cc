// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#include "crawler/robots.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace dse {
namespace crawler {

namespace {

std::string ToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                  [](unsigned char c) { return std::tolower(c); });
  return s;
}

std::string Trim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\r");
  return s.substr(start, end - start + 1);
}

// Matches a robots.txt path pattern (supports '*' wildcard and trailing
// '$' end anchor) against a request path. Per the robots.txt convention,
// a pattern with no '$' anchor matches as a *prefix* -- "Disallow: /private"
// must block "/private/secrets" -- while '*' still matches any run of
// characters within that prefix.
bool MatchesPattern(const std::string& pattern, const std::string& path) {
  bool anchored_end = !pattern.empty() && pattern.back() == '$';
  std::string pat = anchored_end ? pattern.substr(0, pattern.size() - 1)
                                  : pattern;

  size_t pi = 0, si = 0;
  size_t star_idx = std::string::npos, match_idx = 0;
  while (si < path.size()) {
    if (!anchored_end && pi == pat.size()) {
      return true;  // pattern fully consumed -> prefix match succeeds
    }
    if (pi < pat.size() && (pat[pi] == path[si])) {
      pi++; si++;
    } else if (pi < pat.size() && pat[pi] == '*') {
      star_idx = pi++;
      match_idx = si;
    } else if (star_idx != std::string::npos) {
      pi = star_idx + 1;
      si = ++match_idx;
    } else {
      return false;
    }
  }
  while (pi < pat.size() && pat[pi] == '*') pi++;
  if (anchored_end) return pi == pat.size() && si == path.size();
  return pi == pat.size();
}

}  // namespace

RobotsPolicy RobotsPolicy::Parse(const std::string& content,
                                  const std::string& user_agent) {
  RobotsPolicy policy;
  std::istringstream stream(content);
  std::string line;

  std::string current_agent;
  bool group_applies = false;
  bool any_group_seen = false;
  std::vector<Rule> wildcard_rules;
  std::vector<Rule> matched_rules;
  bool found_specific_match = false;
  std::string ua_lower = ToLower(user_agent);

  while (std::getline(stream, line)) {
    auto hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);
    line = Trim(line);
    if (line.empty()) continue;

    auto colon = line.find(':');
    if (colon == std::string::npos) continue;
    std::string key = ToLower(Trim(line.substr(0, colon)));
    std::string value = Trim(line.substr(colon + 1));

    if (key == "user-agent") {
      any_group_seen = true;
      current_agent = ToLower(value);
      group_applies = (current_agent == "*" || ua_lower.find(current_agent) !=
                                                     std::string::npos);
      continue;
    }
    if (!any_group_seen) continue;

    if (key == "disallow" || key == "allow") {
      Rule rule{value, key == "allow"};
      if (rule.pattern.empty() && !rule.allow) continue;  // Disallow: (empty) = allow all
      if (current_agent == "*") {
        wildcard_rules.push_back(rule);
      } else if (group_applies) {
        matched_rules.push_back(rule);
        found_specific_match = true;
      }
    } else if (key == "crawl-delay") {
      try {
        double seconds = std::stod(value);
        policy.crawl_delay_ = std::chrono::milliseconds(
            static_cast<long long>(seconds * 1000));
      } catch (...) {}
    } else if (key == "sitemap") {
      policy.sitemaps_.push_back(value);
    }
  }

  policy.rules_ = found_specific_match ? matched_rules : wildcard_rules;
  return policy;
}

bool RobotsPolicy::IsAllowed(const std::string& path) const {
  const Rule* best = nullptr;
  size_t best_len = 0;
  for (const auto& rule : rules_) {
    if (MatchesPattern(rule.pattern, path) && rule.pattern.size() >= best_len) {
      best = &rule;
      best_len = rule.pattern.size();
    }
  }
  if (best == nullptr) return true;  // No matching rule: allowed by default.
  return best->allow;
}

}  // namespace crawler
}  // namespace dse
