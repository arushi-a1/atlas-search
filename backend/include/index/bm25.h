// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT
//
// BM25F-style ranker: scores body and title fields with independent
// length normalization and a title boost weight, then combines. Supports
// quoted phrase clauses by intersecting positional postings.

#ifndef DISTRIBUTED_SEARCH_INDEX_BM25_H_
#define DISTRIBUTED_SEARCH_INDEX_BM25_H_

#include <string>
#include <vector>

#include "index/inverted_index.h"

namespace dse {
namespace index {

struct SearchResult {
  DocId doc_id;
  double score;
  std::string url;
  std::string title;
  std::string snippet;
};

struct Bm25Params {
  double k1 = 1.2;
  double b = 0.75;
  double title_weight = 2.5;  // title term matches score higher than body
};

// A parsed query: a list of "clauses" where each clause is either a single
// term or a phrase (multiple terms that must appear contiguously).
struct QueryClause {
  std::vector<std::string> terms;  // size 1 = single term, >1 = phrase
  bool is_phrase = false;
};

class QueryParser {
 public:
  // Parses free text with optional "quoted phrases" into clauses.
  static std::vector<QueryClause> Parse(const std::string& query);
};

class Bm25Ranker {
 public:
  Bm25Ranker(const InvertedIndex& index, Bm25Params params = {});

  std::vector<SearchResult> Search(const std::string& query, size_t top_k) const;

 private:
  double ScoreTerm(const std::string& term, const Posting& posting) const;
  bool PhraseMatches(const std::vector<QueryClause>& clauses, DocId doc_id) const;

  const InvertedIndex& index_;
  Bm25Params params_;
};

}  // namespace index
}  // namespace dse

#endif  // DISTRIBUTED_SEARCH_INDEX_BM25_H_
