// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#include "test_framework.h"

#include "index/bm25.h"
#include "index/inverted_index.h"

using dse::index::Bm25Ranker;
using dse::index::IndexedDocument;
using dse::index::InvertedIndex;

namespace {
void PopulateSampleIndex(InvertedIndex* index) {
  index->AddDocument({"https://a.example/1", "Distributed Systems",
                       "a distributed system coordinates many machines to act as one"});
  index->AddDocument({"https://a.example/2", "Cooking Pasta",
                       "boil water then add pasta and salt for great flavor"});
  index->AddDocument({"https://a.example/3", "Search Engines",
                       "a search engine indexes the web and ranks documents by relevance "
                       "a distributed search engine spreads crawling across many machines"});
}
}  // namespace

TEST(InvertedIndexTracksDocumentCount) {
  InvertedIndex index;
  PopulateSampleIndex(&index);
  ASSERT_EQ(index.DocumentCount(), 3u);
}

TEST(InvertedIndexReturnsPostingsForKnownTerm) {
  InvertedIndex index;
  PopulateSampleIndex(&index);
  const auto* postings = index.GetPostings("distributed");
  ASSERT_TRUE(postings != nullptr);
  ASSERT_EQ(postings->size(), 2u);  // docs 1 and 3
}

TEST(InvertedIndexReturnsNullForUnknownTerm) {
  InvertedIndex index;
  PopulateSampleIndex(&index);
  ASSERT_TRUE(index.GetPostings("xyznonexistent") == nullptr);
}

TEST(Bm25RanksMostRelevantDocumentFirst) {
  InvertedIndex index;
  PopulateSampleIndex(&index);
  Bm25Ranker ranker(index);
  auto results = ranker.Search("distributed search engine", 3);
  ASSERT_TRUE(!results.empty());
  ASSERT_EQ(results[0].url, "https://a.example/3");
}

TEST(Bm25TitleMatchBoostsScore) {
  InvertedIndex index;
  PopulateSampleIndex(&index);
  Bm25Ranker ranker(index);
  auto results = ranker.Search("cooking pasta", 3);
  ASSERT_TRUE(!results.empty());
  ASSERT_EQ(results[0].url, "https://a.example/2");
}

TEST(PhraseSearchRequiresExactAdjacency) {
  InvertedIndex index;
  index.AddDocument({"https://p.example/1", "Doc A", "the quick brown fox jumps"});
  index.AddDocument({"https://p.example/2", "Doc B", "quick and then later a brown thing"});
  Bm25Ranker ranker(index);

  auto phrase_results = ranker.Search("\"quick brown\"", 10);
  ASSERT_EQ(phrase_results.size(), 1u);
  ASSERT_EQ(phrase_results[0].url, "https://p.example/1");

  auto bag_results = ranker.Search("quick brown", 10);
  ASSERT_EQ(bag_results.size(), 2u);  // no phrase constraint -> both match
}

TEST(EmptyIndexReturnsNoResults) {
  InvertedIndex index;
  Bm25Ranker ranker(index);
  auto results = ranker.Search("anything", 10);
  ASSERT_TRUE(results.empty());
}
