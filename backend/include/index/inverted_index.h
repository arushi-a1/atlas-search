// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT
//
// A thread-safe, in-memory inverted index with positional postings
// (needed for phrase search) and per-field term frequencies (title vs.
// body) used by the BM25F-style ranker.

#ifndef DISTRIBUTED_SEARCH_INDEX_INVERTED_INDEX_H_
#define DISTRIBUTED_SEARCH_INDEX_INVERTED_INDEX_H_

#include <cstdint>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace dse {
namespace index {

using DocId = uint32_t;

struct DocumentMeta {
  std::string url;
  std::string title;
  std::string snippet;
  uint32_t body_length = 0;   // in tokens, for BM25 length normalization
  uint32_t title_length = 0;
};

struct Posting {
  DocId doc_id;
  uint32_t body_freq = 0;
  uint32_t title_freq = 0;
  std::vector<uint32_t> body_positions;   // for phrase search
  std::vector<uint32_t> title_positions;
};

struct IndexedDocument {
  std::string url;
  std::string title;
  std::string body_text;   // already-extracted visible text
};

class InvertedIndex {
 public:
  // Tokenizes, indexes, and assigns a DocId. Returns the assigned id.
  DocId AddDocument(const IndexedDocument& doc);

  // Returns postings for a term, or nullptr if the term is unseen.
  const std::vector<Posting>* GetPostings(const std::string& term) const;

  const DocumentMeta* GetDocumentMeta(DocId id) const;

  size_t DocumentCount() const;
  double AverageBodyLength() const;
  uint32_t DocFrequency(const std::string& term) const;

 private:
  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, std::vector<Posting>> postings_;
  std::vector<DocumentMeta> documents_;
  uint64_t total_body_length_ = 0;
};

}  // namespace index
}  // namespace dse

#endif  // DISTRIBUTED_SEARCH_INDEX_INVERTED_INDEX_H_
