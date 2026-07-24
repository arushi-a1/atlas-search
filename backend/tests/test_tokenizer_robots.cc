// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#include "test_framework.h"

#include "crawler/robots.h"
#include "index/tokenizer.h"

using dse::crawler::RobotsPolicy;
using dse::index::Tokenizer;

TEST(TokenizeLowercasesAndSplitsOnPunctuation) {
  auto tokens = Tokenizer::Tokenize("Hello, World! It's BM25-ranking.");
  std::vector<std::string> expected = {"hello", "world", "it", "s", "bm25", "ranking"};
  ASSERT_EQ(tokens.size(), expected.size());
  for (size_t i = 0; i < tokens.size(); ++i) ASSERT_EQ(tokens[i], expected[i]);
}

TEST(TokenizeHandlesEmptyInput) {
  auto tokens = Tokenizer::Tokenize("");
  ASSERT_TRUE(tokens.empty());
}

TEST(StopWordDetection) {
  ASSERT_TRUE(Tokenizer::IsStopWord("the"));
  ASSERT_TRUE(!Tokenizer::IsStopWord("search"));
}

TEST(RobotsDisallowsExactPath) {
  std::string robots_txt =
      "User-agent: *\n"
      "Disallow: /private\n"
      "Allow: /private/public-page\n";
  RobotsPolicy policy = RobotsPolicy::Parse(robots_txt, "DistributedSearchBot/1.0");
  ASSERT_TRUE(!policy.IsAllowed("/private/secrets"));
  ASSERT_TRUE(policy.IsAllowed("/private/public-page"));
  ASSERT_TRUE(policy.IsAllowed("/about"));
}

TEST(RobotsWildcardPattern) {
  std::string robots_txt =
      "User-agent: *\n"
      "Disallow: /*.pdf$\n";
  RobotsPolicy policy = RobotsPolicy::Parse(robots_txt, "DistributedSearchBot/1.0");
  ASSERT_TRUE(!policy.IsAllowed("/files/report.pdf"));
  ASSERT_TRUE(policy.IsAllowed("/files/report.pdf.html"));
}

TEST(RobotsCrawlDelayParsed) {
  std::string robots_txt = "User-agent: *\nCrawl-delay: 2\n";
  RobotsPolicy policy = RobotsPolicy::Parse(robots_txt, "DistributedSearchBot/1.0");
  ASSERT_EQ(policy.CrawlDelay().count(), 2000);
}

TEST(RobotsSpecificAgentOverridesWildcard) {
  std::string robots_txt =
      "User-agent: *\n"
      "Disallow: /\n"
      "User-agent: DistributedSearchBot\n"
      "Allow: /\n";
  RobotsPolicy policy = RobotsPolicy::Parse(robots_txt, "DistributedSearchBot/1.0");
  ASSERT_TRUE(policy.IsAllowed("/anything"));
}
