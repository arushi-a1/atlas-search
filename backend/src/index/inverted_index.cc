// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT

#include "index/inverted_index.h"

#include <algorithm>
#include <mutex>

#include "index/tokenizer.h"

namespace dse {
namespace index {

DocId InvertedIndex::AddDocument(const IndexedDocument& doc) {
  std::vector<std::string> body_tokens = Tokenizer::Tokenize(doc.body_text);
  std::vector<std::string> title_tokens = Tokenizer::Tokenize(doc.title);

  std::unique_lock<std::shared_mutex> lock(mutex_);
  DocId id = static_cast<DocId>(documents_.size());

  DocumentMeta meta;
  meta.url = doc.url;
  meta.title = doc.title;
  meta.snippet = doc.body_text.substr(0, std::min<size_t>(280, doc.body_text.size()));
  meta.body_length = static_cast<uint32_t>(body_tokens.size());
  meta.title_length = static_cast<uint32_t>(title_tokens.size());
  documents_.push_back(std::move(meta));
  total_body_length_ += body_tokens.size();

  std::unordered_map<std::string, Posting> per_term;
  for (uint32_t pos = 0; pos < body_tokens.size(); ++pos) {
    Posting& p = per_term[body_tokens[pos]];
    p.doc_id = id;
    p.body_freq++;
    p.body_positions.push_back(pos);
  }
  for (uint32_t pos = 0; pos < title_tokens.size(); ++pos) {
    Posting& p = per_term[title_tokens[pos]];
    p.doc_id = id;
    p.title_freq++;
    p.title_positions.push_back(pos);
  }

  for (auto& [term, posting] : per_term) {
    postings_[term].push_back(std::move(posting));
  }

  return id;
}

const std::vector<Posting>* InvertedIndex::GetPostings(
    const std::string& term) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  auto it = postings_.find(term);
  if (it == postings_.end()) return nullptr;
  return &it->second;
}

const DocumentMeta* InvertedIndex::GetDocumentMeta(DocId id) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  if (id >= documents_.size()) return nullptr;
  return &documents_[id];
}

size_t InvertedIndex::DocumentCount() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return documents_.size();
}

double InvertedIndex::AverageBodyLength() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  if (documents_.empty()) return 0.0;
  return static_cast<double>(total_body_length_) / documents_.size();
}

uint32_t InvertedIndex::DocFrequency(const std::string& term) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  auto it = postings_.find(term);
  if (it == postings_.end()) return 0;
  return static_cast<uint32_t>(it->second.size());
}

}  // namespace index
}  // namespace dse
